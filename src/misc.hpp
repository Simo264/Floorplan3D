#pragma once

#include "spatial_hashing.hpp"
#include "types.hpp"

#include <vector>

bool are_vectors_parallel(glm::dvec2 v1, glm::dvec2 v2, f64 eps = 1e-4);

f64 calculate_signed_area(const std::vector<glm::dvec2>& contour);

// Normalizes all segment coordinates according to the drawing's measurement unit.
void normalize_segments(f64 unit, std::vector<Segment>& segments);

// Centers the mesh by moving all vertices along their spatial coordinates.
void center_mesh(std::vector<Segment>& walls, std::vector<Segment>& doors, std::vector<Segment>& windows);

// Finds the four vertices that define the wall strip around a gap.
WallVertices get_wall_vertices(glm::dvec2 gap_start,
                               glm::dvec2 gap_end,
                               const SpatialHash& hash,
                               const std::vector<Edge>& edges,
                               const std::vector<glm::dvec2>& vertices);


// Retrieves the topological neighbors connected to a specific vertex.
// Searches through the edge list to find the two adjacent vertices connected to the
// target vertex. In a manifold wall layout, each inner corner or wall endpoint vertex
// is expected to connect exactly to two edges (forming the inner and outer faces of the wall).
std::array<VertexId, 2> find_neighboors(VertexId vertex, const std::vector<Edge>& edges);

// Identifies the corresponding wall-thickness vertex on the opposite side of a wall.
// Evaluates the two topological neighbors of a given vertex to determine which one points
// across the wall thickness rather than continuing along the wall length. It computes the
// absolute dot product between the door/wall longitudinal direction and the normalized direction
// vectors of both neighbors. The neighbor with the lowest dot product (closest to being
// perpendicular, i.e., 0) is selected as the correct opposite vertex.
VertexId get_adjacent_vertex(glm::dvec2 wall_dir,
                             VertexId vertex_id,
                             std::array<VertexId, 2> vertex_neighbors,
                             const std::vector<glm::dvec2>& vertices);

// Closes the wall around a door or window gap.
void close_wall_gap(glm::dvec2 gap_start,
                    glm::dvec2 gap_end,
                    SegmentLayer type,
                    SpatialHash& hash,
                    std::vector<Edge>& edges,
                    f64 width_scale);

// OpeningInstance compute_opening_instance(const Face& face, OpeningType type, f32 z_min, f32 z_max);
