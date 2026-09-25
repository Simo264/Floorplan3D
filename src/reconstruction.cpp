#include "reconstruction.hpp"

#include <GLFW/glfw3.h>

#include <format>
#include <algorithm>
#include <cstdlib>
#include <stdexcept>
#include <vector>
#include <cstdio>
#include <print>

#include <glm/ext/vector_float4.hpp>
#include <glm/glm.hpp>

#include "graphics/static_mesh.hpp"
#include "io/drw_parser.hpp"
#include "types.hpp"
#include "misc.hpp"

#include "dbscan.h"


ParsingResult parse_dxf(bool verbose, const std::filesystem::path& file, f64 unit_scale)
{
  auto parser = DRWParser{ verbose };
  auto dxf = dxfRW(file.string().c_str());
  if (!dxf.read(&parser, false))
    throw std::runtime_error(std::format("Error reading DXF file `{}` (code: {})", file.string(), static_cast<i32>(dxf.getError())));

  auto doors_before = parser.doors.size();
  parser.remove_duplicate_segments(parser.doors);
  auto doors_after = parser.doors.size();
  auto doors_removed = doors_before - doors_after;
  auto total_segments = parser.walls.size() + parser.doors.size() + parser.windows.size();

  if (doors_removed > 0)
    std::println("- removed {} duplicate door segments", doors_removed);

  std::println("- number of wall segments: {}", parser.walls.size());
  std::println("- number of door segments: {}", parser.doors.size());
  std::println("- number of window segments: {}", parser.windows.size());
  std::println("- total segments: {}", total_segments);
  std::println("- total vertices: {}", total_segments * 2);

  normalize_segments(unit_scale, parser.walls);
  normalize_segments(unit_scale, parser.doors);
  normalize_segments(unit_scale, parser.windows);
  center_mesh(parser.walls, parser.doors, parser.windows);

  auto result = ParsingResult{};
  result.walls = std::move(parser.walls);
  result.doors = std::move(parser.doors);
  result.windows = std::move(parser.windows);
  return result;
}

SnappingResult vertex_snapping(const std::vector<Segment>& segments, f64 snap_eps)
{
  auto hash = SpatialHash{ snap_eps };
  auto edges = std::vector<Edge>{};
  for (const auto& seg : segments)
  {
    auto v1 = hash.snap(seg.start);
    auto v2 = hash.snap(seg.end);
    if (v1 != v2)
      edges.push_back(Edge{ v1, v2, seg.layer });
  }

  auto result = SnappingResult{};
  result.hash = std::move(hash);
  result.edges = std::move(edges);
  return result;
}

void doors_reconstruction(std::vector<Segment>& doors, SpatialHash& hash, std::vector<Edge>& edges, i32 door_width)
{
  auto& vertices = hash.vertices();
  for (auto i = 0ul; i < doors.size(); ++i)
  {
    auto& door = doors[i];
    auto current_width = glm::distance(door.start, door.end);
    auto width_scale = door_width / current_width;
    if(width_scale == 0.0)
      width_scale = 1.0;

    close_wall_gap(door.start, door.end, SegmentLayer::Door, hash, edges, width_scale);
    door.start = vertices[hash.find_nearest(door.start)];
    door.end   = vertices[hash.find_nearest(door.end)];
  }
}

std::vector<glm::dvec2> sample_segments(const std::vector<Segment>& segments, i32 num_samples)
{
  auto points = std::vector<glm::dvec2>{};
  points.reserve(segments.size());
  for (const auto& s : segments)
  {
    for (auto i = 0; i <= num_samples; ++i)
    {
      auto t = static_cast<f64>(i) / num_samples;
      auto x = s.start.x + t * (s.end.x - s.start.x);
      auto y = s.start.y + t * (s.end.y - s.start.y);
      points.push_back(glm::dvec2{x, y});
    }
  }
  return points;
}

std::vector<std::vector<u32>> calculate_clusters(std::vector<glm::dvec2>& sample_points, f64 eps)
{
  auto dbscan = DBSCAN<glm::dvec2, f64>();
  dbscan.Run(&sample_points, 2, eps, 2);
  return dbscan.Clusters;
}

void windows_reconstruction(std::vector<glm::dvec2>& sample_points,
                            std::vector<std::vector<u32>> clusters,
                            SpatialHash& hash,
                            std::vector<Edge>& edges,
                            f32 window_width)
{
  for (auto i = 0ul; i < clusters.size(); ++i)
  {
    const auto& cluster_indices = clusters[i];
    if (cluster_indices.empty())
      continue;

    auto box = BoundingBox2D(sample_points, cluster_indices);
    auto sides = box.get_long_sides();
    auto longest_side = sides.at(0);
    auto current_width = glm::distance(longest_side.start, longest_side.end);
    auto width_scale = window_width / current_width;
    if(width_scale == 0.0)
      width_scale = 1.0;
    close_wall_gap(longest_side.start, longest_side.end, SegmentLayer::Window, hash, edges, width_scale);
  }
}


ReconstructionResult build_mesh(const std::vector<Face>& faces, const Config& config)
{
  auto mesh_vertices       = std::vector<Vertex>{};
  auto mesh_floor_indices  = std::vector<u32>{};
  auto mesh_wall_indices   = std::vector<u32>{};
  auto result              = ReconstructionResult{};

  auto ceil_height         = config.ceil_height;
  auto door_height         = config.door_height;
  auto window_sill         = config.window_sill_height;
  auto window_height       = config.window_height;
  auto wall_tex_scaling    = config.wall_texture_scaling;
  auto floor_tex_scaling   = config.floor_texture_scaling;

  auto wall_faces = std::ranges::views::filter(faces, [](auto face) { return face.type == FaceType::Wall; });
  auto door_faces = std::ranges::views::filter(faces, [](auto face) { return face.type == FaceType::Door; });
  auto window_faces = std::ranges::views::filter(faces, [](auto face) { return face.type == FaceType::Window; });
  auto floor_faces = std::ranges::views::filter(faces, [](auto face) { return face.type == FaceType::Floor; });

  // =======================
  // Triangulate floor and ceiling
  // =======================
  for(const auto& face : floor_faces)
  {
    // triangulate floor
    face.triangulate(mesh_vertices, mesh_floor_indices, 0.f, floor_tex_scaling, true);
    // triangulate ceiling
    face.triangulate(mesh_vertices, mesh_wall_indices, ceil_height, wall_tex_scaling, false);
  }

  // =======================
  // Extrude walls
  // =======================
  for(const auto& face : wall_faces)
  {
    face.extrude(mesh_vertices, mesh_wall_indices, 0.f, ceil_height, wall_tex_scaling);

    // triangulate floor
    //face.triangulate(mesh_vertices, mesh_wall_indices, 0.f, wall_tex_scaling, false);
    // triangulate ceiling
    // face.triangulate(mesh_vertices, mesh_wall_indices, ceil_height, wall_tex_scaling, true);
  }

  // =======================
  // Extrude doors
  // =======================
  for(const auto& face : door_faces)
  {
    face.extrude(mesh_vertices, mesh_wall_indices, door_height, ceil_height, wall_tex_scaling);

    // triangulate floor
    //face.triangulate(mesh_vertices, mesh_wall_indices, 0.f, wall_tex_scaling, false);
    // triangulate ceiling
    // face.triangulate(mesh_vertices, mesh_wall_indices, ceil_height, wall_tex_scaling, true);

    // triangulate door
    face.triangulate(mesh_vertices, mesh_wall_indices, door_height, wall_tex_scaling, false);

    auto opening = compute_opening_instance(face, OpeningType::Door, 0.0f, door_height);
    result.openings.push_back(opening);
  }

  // =======================
  // Extrude windows
  // =======================
  for(const auto& face : window_faces)
  {
    // extrude bottom
    face.extrude(mesh_vertices, mesh_wall_indices, 0.0f, window_sill, wall_tex_scaling);
    // extrude top
    face.extrude(mesh_vertices, mesh_wall_indices, window_height, ceil_height, wall_tex_scaling);

    // triangulate floor
    // face.triangulate(mesh_vertices, mesh_wall_indices, 0.f, wall_tex_scaling, false);
    // triangulate ceiling
    // face.triangulate(mesh_vertices, mesh_wall_indices, ceil_height, wall_tex_scaling, true);

    // triangulate window_sill
    face.triangulate(mesh_vertices, mesh_wall_indices, window_sill, wall_tex_scaling, true);
    // triangulate window_height
    face.triangulate(mesh_vertices, mesh_wall_indices, window_height, wall_tex_scaling, false);

    auto opening = compute_opening_instance(face, OpeningType::Window, window_sill, window_height);
    result.openings.push_back(opening);
  }

  auto floor_range = PrimitiveRange{ 0, static_cast<u32>(mesh_floor_indices.size()), MaterialType::Floor };
  auto all_indices = std::vector<u32>{};
  all_indices.reserve(mesh_floor_indices.size() + mesh_wall_indices.size());
  all_indices.insert(all_indices.end(), mesh_floor_indices.begin(), mesh_floor_indices.end());

  auto wall_range = PrimitiveRange{ static_cast<u32>(all_indices.size()), static_cast<u32>(mesh_wall_indices.size()), MaterialType::Wall };
  all_indices.insert(all_indices.end(), mesh_wall_indices.begin(), mesh_wall_indices.end());

  result.mesh_vertices = std::move(mesh_vertices);
  result.mesh_indices  = std::move(all_indices);
  result.primitives = { floor_range, wall_range };
  return result;
}
