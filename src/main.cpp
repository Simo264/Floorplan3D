#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <filesystem>
#include <print>
#include <format>
#include <memory>
#include <vector>
#include <stdexcept>

#include "dump.hpp"
#include "types.hpp"
#include "reconstruction.hpp"
#include "arrangement.hpp"
#include "mesh_visualizer.hpp"
#include "io/gltf_exporter.hpp"
#include "io/config_loader.hpp"
#include "graphics/static_mesh.hpp"

#include <glm/trigonometric.hpp>
#include <glm/geometric.hpp>


static auto create_floor_face(BoundingBox2D house_bbox)
{
  auto floor_face = Face{};
  floor_face.vertices = {
    glm::dvec2(house_bbox.min.x, house_bbox.min.y),
    glm::dvec2(house_bbox.max.x, house_bbox.min.y),
    glm::dvec2(house_bbox.max.x, house_bbox.max.y),
    glm::dvec2(house_bbox.min.x, house_bbox.max.y)
  };
  floor_face.type = FaceType::Floor;
  return floor_face;
}

int main(int argc, char** argv)
{
  if (argc != 2)
    throw std::runtime_error("Missing argument: <config>.json");

  auto config = Config(argv[1]);
  static auto counter = 0;
  std::string output_name;

  // =======================================================
  // Step 1: parsing
  // =======================================================
  std::println("\n=========== Step 1: parsing ===========\n");
  std::println("DXF file {} ...", config.dxf_path.string());
  ParsingResult parsing_result = parse_dxf(false, config.dxf_path, config.unit_scale);
  auto house_bbox = BoundingBox2D(parsing_result.walls);
  dump_segments_csv(parsing_result.walls, "out/walls.csv");
  dump_segments_csv(parsing_result.doors, "out/doors.csv");
  dump_segments_csv(parsing_result.windows, "out/windows.csv");
  output_name = std::format("out/segments_{:04d}.jpg", counter++);
  run_python_script("plot_segments.py", output_name);
  std::filesystem::remove("out/walls.csv");
  std::filesystem::remove("out/doors.csv");
  std::filesystem::remove("out/windows.csv");

  // =======================================================
  // Step 2: vertex snapping
  // =======================================================
  std::println("\n=========== Step 2: vertex snapping ===========\n");
  SnappingResult snapping_result{};
  {
    auto wall_vertices = std::vector<glm::dvec2>{};
    wall_vertices.reserve(parsing_result.walls.size() * 2);
    for (const auto& segment : parsing_result.walls)
    {
      wall_vertices.push_back(segment.start);
      wall_vertices.push_back(segment.end);
    }
    auto num_vertices_before = wall_vertices.size();
    std::println("Before snapping: {} vertices", num_vertices_before);
    dump_vertices_csv(wall_vertices, "out/vertices.csv");
    output_name = std::format("out/vertices_{:04d}.jpg", counter++);
    run_python_script("plot_vertices.py", output_name);
    std::filesystem::remove("out/vertices.csv");

    snapping_result = vertex_snapping(parsing_result.walls, config.snap_eps);
    auto num_vertices_after = snapping_result.hash.vertices().size();
    auto merged = num_vertices_before - num_vertices_after;
    std::println("After snapping: {} vertices -> {} merged", num_vertices_after, merged);
    dump_vertices_csv(snapping_result.hash.vertices(), "out/vertices.csv");
    output_name = std::format("out/vertices_{:04d}.jpg", counter++);
    run_python_script("plot_vertices.py", output_name);
    std::filesystem::remove("out/vertices.csv");
  }

  auto& hash = snapping_result.hash;
  auto& edges = snapping_result.edges;

  // =======================================================
  // Step 3: doors reconstruction
  // =======================================================
  std::println("\n=========== Step 3: doors reconstruction ===========\n");
  if(!parsing_result.doors.empty())
  {
    auto door_width = config.door_width;
    std::println("Before doors reconstruction: vertices = {} edges = {}", hash.vertices().size(), edges.size());
    doors_reconstruction(parsing_result.doors, hash, edges, door_width);
    std::println("After doors reconstruction: vertices = {} edges = {}", hash.vertices().size(), edges.size());
    // dump segments for debugging
    {
      const auto& vertices = hash.vertices();
      auto walls_segments = std::vector<Segment>{};
      auto doors_segments = std::vector<Segment>{};
      for (const auto& edge : edges)
      {
        auto& p1 = vertices[edge.v1];
        auto& p2 = vertices[edge.v2];
        auto seg = Segment{ p1, p2, edge.layer };
        if(edge.layer == SegmentLayer::Wall)
          walls_segments.push_back(seg);
        else if(edge.layer == SegmentLayer::Door)
          doors_segments.push_back(seg);
      }
      dump_segments_csv(walls_segments, "out/walls.csv");
      dump_segments_csv(doors_segments, "out/doors.csv");
      output_name = std::format("out/segments_{:04d}.jpg", counter++);
      run_python_script("plot_segments.py", output_name);
      std::filesystem::remove("out/vertices.csv");
      std::filesystem::remove("out/doors.csv");
    }
  }
  else
    std::println("No doors to reconstruct.");

  // =======================================================
  // Step 4: windows reconstruction
  // =======================================================
  std::println("\n=========== Step 4: windows reconstruction ===========\n");
  if(!parsing_result.windows.empty())
  {
    auto& windows = parsing_result.windows;
    auto num_samples = config.cluster_num_samples;
    auto eps = config.cluster_eps;

    // Sample points from window segments and calculate clusters
    auto sample_points = sample_segments(windows, num_samples);
    auto clusters = calculate_clusters(sample_points, eps);
    std::println("sample_points: {}, clusters: {}", sample_points.size(), clusters.size());
    dump_clusters_csv(sample_points, clusters, "out/clusters.csv");
    output_name = std::format("out/clusters_{:04d}.jpg", counter++);
    run_python_script("plot_clusters.py", output_name);
    std::filesystem::remove("out/clusters.csv");

    auto window_width = config.window_width;
    windows_reconstruction(sample_points, clusters, hash, edges, window_width);
    // dump segments for debugging
    {
      const auto& vertices = hash.vertices();
      auto walls_segments = std::vector<Segment>{};
      auto doors_segments = std::vector<Segment>{};
      auto windows_segments = std::vector<Segment>{};
      for (const auto& edge : edges)
      {
        auto& p1 = vertices[edge.v1];
        auto& p2 = vertices[edge.v2];
        auto seg = Segment{ p1, p2, edge.layer };
        if(edge.layer == SegmentLayer::Wall)
          walls_segments.push_back(seg);
        else if(edge.layer == SegmentLayer::Door)
          doors_segments.push_back(seg);
        else if(edge.layer == SegmentLayer::Window)
          windows_segments.push_back(seg);
      }
      dump_segments_csv(walls_segments, "out/walls.csv");
      dump_segments_csv(doors_segments, "out/doors.csv");
      dump_segments_csv(windows_segments, "out/windows.csv");
      output_name = std::format("out/segments_{:04d}.jpg", counter++);
      run_python_script("plot_segments.py", output_name);
      std::filesystem::remove("out/vertices.csv");
      std::filesystem::remove("out/doors.csv");
      std::filesystem::remove("out/windows.csv");
    }
  }
  else
    std::println("No windows to reconstruct.");

  // =======================================================
  // Step 5: planar straight line graph and face extraction
  // =======================================================
  std::println("\n=========== Step 5: face extraction ===========\n");
  auto arrangement = build_arrangement(hash.vertices(), edges);
  std::println("- Number of faces: {}", arrangement.number_of_faces());
  std::println("- Number of vertices: {}", arrangement.number_of_vertices());
  std::println("- Number of edges: {}", arrangement.number_of_edges());
  auto faces = extract_faces(arrangement);
  std::println("- Number of extracted faces: {}", faces.size());

  dump_faces_csv(faces, "out/faces.csv");
  output_name = std::format("out/faces_{:04d}.png", counter++);
  run_python_script("plot_faces.py", output_name);
  std::filesystem::remove("out/faces.csv");

  // =======================================================
  // Step 6: mesh building
  // =======================================================
  std::println("\n=========== Step 6: mesh building ===========\n");
  // remove all FLOOR faces and push only one quad for floor
  std::erase_if(faces, [](auto face) { return face.type == FaceType::Floor; });
  auto floor_face = create_floor_face(house_bbox);
  faces.push_back(std::move(floor_face));
  auto build_result = build_mesh(faces, config);
  auto num_vertices = build_result.mesh_vertices.size();
  auto num_indices = build_result.mesh_indices.size();
  std::println("- Number of mesh vertices: {}", num_vertices);
  std::println("- Number of mesh indices: {}", num_indices);
  std::println("- VBO size: {}", num_vertices * sizeof(Vertex));
  std::println("- EBO size: {}", num_indices * sizeof(u32));
  std::println("- Total size: {} bytes", (num_vertices * sizeof(Vertex)) + (num_indices * sizeof(u32)));

  // =======================================================
  // Step 7: export mesh
  // =======================================================
  std::println("\n=========== Step 7: export mesh ===========\n");
  // e.g. out/draftperson_Floor_Plan.gltf
  auto gltf_path = std::filesystem::path("out") / config.dxf_path.filename().replace_extension("gltf");
  export_to_gltf(build_result, gltf_path);
  std::println("- GLTF mesh file: {}", gltf_path.string());

  // e.g. out/draftperson_Floor_Plan_openings.json
  auto json_filename = config.dxf_path.stem().string() + "_openings.json";
  auto json_path = std::filesystem::path("out") / json_filename;
  export_opening_placeholders(build_result, json_path);
  std::println("- JSON placeholder file: {}", json_path.string());

  // =======================================================
  // Step 8: visualize mesh with openGL
  // =======================================================

  MeshVisualizer visualizer(1024, 768);
  visualizer.camera.eye = { 0.0f, 2.0f, 5.0f };
  // visualizer.camera.set_orientation(glm::radians(glm::vec3{ 170.f, 40.f, 180.f }));

  auto static_mesh = std::make_shared<StaticMesh>(build_result.mesh_vertices.data(),
                                                  build_result.mesh_vertices.size(),
                                                  build_result.mesh_indices.data(),
                                                  build_result.mesh_indices.size());

  visualizer.render(static_mesh, build_result.primitives);
}
