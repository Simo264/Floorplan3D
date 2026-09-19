#pragma once

#include "types.hpp"
#include "spatial_hashing.hpp"
#include "io/config_loader.hpp"
#include "graphics/static_mesh.hpp"

#include <vector>
#include <filesystem>

struct ParsingResult
{
  std::vector<Segment> walls, doors, windows;
};

struct SnappingResult
{
  SpatialHash hash;
  std::vector<Edge> edges;
};

struct ReconstructionResult
{
  std::vector<Vertex> mesh_vertices;
  std::vector<u32> mesh_indices;
  std::vector<PrimitiveRange> primitives;
  std::vector<OpeningInstance> openings;
};

ParsingResult parse_dxf(bool verbose, const std::filesystem::path& file, f64 unit_scale);
SnappingResult vertex_snapping(const std::vector<Segment>& segments, f64 snap_eps);
void doors_reconstruction(std::vector<Segment>& doors, SpatialHash& hash, std::vector<Edge>& edges, i32 door_width);

std::vector<glm::dvec2> sample_segments(const std::vector<Segment>& segments, i32 num_samples);
std::vector<std::vector<u32>> calculate_clusters(std::vector<glm::dvec2>& sample_points, f64 eps);
void windows_reconstruction(std::vector<glm::dvec2>& sample_points,
                            std::vector<std::vector<u32>> clusters,
                            SpatialHash& hash,
                            std::vector<Edge>& edges,
                            f32 window_width);

ReconstructionResult build_mesh(const std::vector<Face>& faces, const Config& config);
