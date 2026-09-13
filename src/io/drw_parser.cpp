#include "drw_parser.hpp"

#include <glm/ext/vector_double2.hpp>
#include <glm/trigonometric.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>

#include <print>
#include <string>
#include <string_view>
#include <map>
#include <algorithm>
#include <execution>

// =============================
// Privates
// =============================

SegmentLayer DRWParser::classify_layer(std::string_view name)
{
  if (name.compare("A-DOOR") == 0)      return SegmentLayer::Door;
  if (name.compare("A-WALL") == 0)      return SegmentLayer::Wall;
  if (name.compare("A-WINDOW") == 0)    return SegmentLayer::Window;
  return SegmentLayer::None;
}

// =============================
// Publics
// =============================

void DRWParser::remove_duplicate_segments(std::vector<Segment>& segments)
{
  auto canonicalize = [](const Segment& s) -> std::pair<glm::dvec2, glm::dvec2> {
    if (s.start.x < s.end.x || (s.start.x == s.end.x && s.start.y < s.end.y))
      return {s.start, s.end};
    return {s.end, s.start};
  };
  auto are_segments_equal = [&](const Segment& lhs, const Segment& rhs) -> bool {
    if (lhs.layer != rhs.layer) return false;
    auto [l_start, l_end] = canonicalize(lhs);
    auto [r_start, r_end] = canonicalize(rhs);
    return l_start == r_start && l_end == r_end;
  };
  auto segment_less = [&](const Segment& lhs, const Segment& rhs) -> bool {
    if (lhs.layer != rhs.layer)
      return lhs.layer < rhs.layer;

    auto [l_start, l_end] = canonicalize(lhs);
    auto [r_start, r_end] = canonicalize(rhs);

    if (l_start.x != r_start.x) return l_start.x < r_start.x;
    if (l_start.y != r_start.y) return l_start.y < r_start.y;
    if (l_end.x != r_end.x) return l_end.x < r_end.x;
    return l_end.y < r_end.y;
  };

  std::sort(std::execution::par, segments.begin(), segments.end(), segment_less);

  auto [first, last] = std::ranges::unique(segments, are_segments_equal);

  segments.erase(first, last);
}


void DRWParser::addLine(const DRW_Line& data)
{
  auto layer_type = classify_layer(data.layer);
  if(m_verbose)
    std::println("[Line] name:`{}`", data.layer);
  auto segment = Segment{};
  segment.start = glm::dvec2{ data.basePoint.x, data.basePoint.y };
  segment.end = glm::dvec2{ data.secPoint.x, data.secPoint.y };
  segment.layer = layer_type;

  if (m_is_parsing_block)
  {
    m_block_vertices[m_current_block_name].push_back(segment);
    return;
  }

  if(layer_type == SegmentLayer::Wall)
    walls.push_back(segment);
  else if(layer_type == SegmentLayer::Window)
    windows.push_back(segment);
  else if (layer_type == SegmentLayer::Door)
    doors.push_back(segment);
}

void DRWParser::addLWPolyline(const DRW_LWPolyline& data)
{
  auto layer_type = classify_layer(data.layer);
  auto is_closed = data.flags & 1;
  auto& vertices = data.vertlist;
  if(m_verbose)
    std::println("[LWPolyline] name:`{}`, vertices:{}, is_closed:{}", data.layer, vertices.size(), is_closed);

  if(vertices.empty())
    return;

  if (m_is_parsing_block)
  {
    auto count = (i32) vertices.size();
    auto num_segments = is_closed ? count : count - 1;
    for (auto i = 0; i < num_segments; ++i)
    {
      auto& v1 = vertices[i];
      auto& v2 = vertices[(i + 1) % count];
      m_block_vertices[m_current_block_name].push_back(Segment{
        .start = glm::dvec2{ v1->x, v1->y },
        .end   = glm::dvec2{ v2->x, v2->y },
        .layer = layer_type
      });
    }
    return;
  }

  // break polyline into segments
  auto to_segments = [&](SegmentLayer lt) -> std::vector<Segment>
  {
    auto count = (i32)vertices.size();
    auto num_segments = is_closed ? count : count - 1;
    std::vector<Segment> segs;
    segs.reserve(num_segments);
    for (auto i = 0; i < num_segments; ++i)
    {
      auto& v1 = vertices[i];
      auto& v2 = vertices[(i + 1) % count];
      segs.push_back(Segment{
        .start = glm::dvec2{ v1->x, v1->y },
        .end   = glm::dvec2{ v2->x, v2->y },
        .layer = lt
      });
    }
    return segs;
  };
  // convert vertices to glm::dvec2
  auto to_points = [&]() -> std::vector<glm::dvec2>
  {
    std::vector<glm::dvec2> pts;
    pts.reserve(vertices.size());
    for (auto& v : vertices)
      pts.emplace_back(v->x, v->y);
    return pts;
  };

  // =============================
  // WALL
  // =============================

  if(layer_type == SegmentLayer::Wall)
  {
    // Break the polyline into segments and add to the walls
    auto segs = to_segments(SegmentLayer::Wall);
    for (auto& seg : segs)
      walls.push_back(seg);
  }

  // =============================
  // WINDOW
  // =============================

  else if(layer_type == SegmentLayer::Window)
  {
    // Break the polyline into segments and add to the windows
    auto segs = to_segments(SegmentLayer::Window);
    for (auto& seg : segs)
      windows.push_back(seg);
  }

  // =============================
  // DOOR
  // =============================

  else if(layer_type == SegmentLayer::Door)
  {
    // Calculate bb from points
    auto pts = to_points();
    auto bbox = BoundingBox2D(pts);
    // We look for the longest side
    auto long_sides = bbox.get_long_sides();
    auto longest_side = long_sides[0];
    longest_side.layer = layer_type;
    doors.push_back(longest_side);
  }
}

void DRWParser::addArc(const DRW_Arc& data)
{
  if (m_is_parsing_block)
  {
    m_block_door_info[m_current_block_name] = DoorBlockInfo{ data.radious };
    return;
  }

  if(m_verbose)
    std::println("[Arc] name:`{}`", data.layer);
  auto layer_type = classify_layer(data.layer);

  auto center = glm::dvec2{ data.basePoint.x, data.basePoint.y };
  auto radius = data.radious;
  auto p1 = glm::dvec2{
    center.x + radius * glm::cos(glm::radians(data.staangle)),
    center.y + radius * glm::sin(glm::radians(data.staangle))
  };
  auto p2 = glm::dvec2{
    center.x + radius * glm::cos(glm::radians(data.endangle)),
    center.y + radius * glm::sin(glm::radians(data.endangle))
  };
  doors.push_back(Segment{ p1, p2, layer_type });
}

void DRWParser::addInsert(const DRW_Insert& data)
{
  const auto& layer_name = data.layer;
  const auto& block_name = data.name;
  auto angle_rad = data.angle; // in radians
  auto cos_angle = std::cos(angle_rad);
  auto sin_angle = std::sin(angle_rad);

  if(m_verbose)
    std::println("[Insert] layer:`{}`, block:`{}`", layer_name, block_name);
  auto layer_type = classify_layer(data.layer);

  // =============================
  // DOOR
  // =============================

  if(layer_type == SegmentLayer::Door)
  {
    auto base_width = 10.0;
    auto it = m_block_door_info.find(block_name);
    if (it != m_block_door_info.end())
      base_width = it->second.radius;

    auto door_width = base_width * data.xscale;
    auto hinge = glm::dvec2{ data.basePoint.x, data.basePoint.y };
    auto tip = glm::dvec2{
      hinge.x + door_width * cos_angle,
      hinge.y + door_width * sin_angle
    };
    doors.push_back(Segment{ hinge, tip, SegmentLayer::Door });
  }

  // =============================
  // WINDOW
  // =============================

  else if(layer_type == SegmentLayer::Window)
  {
    auto it = m_block_vertices.find(block_name);
    if (it == m_block_vertices.end())
      return;

    auto insert_pos = glm::dvec2{ data.basePoint.x, data.basePoint.y };
    auto local_to_world_transformation = [&](const glm::dvec2& local)
    {
      auto scaled = glm::dvec2{ local.x * data.xscale, local.y * data.yscale };
      auto rotated = glm::dvec2{
        scaled.x * cos_angle - scaled.y * sin_angle,
        scaled.x * sin_angle + scaled.y * cos_angle
      };
      return glm::dvec2{ rotated.x + insert_pos.x, rotated.y + insert_pos.y };
    };

    for (const auto& local_segment : it->second)
    {
      auto start = local_to_world_transformation(local_segment.start);
      auto end = local_to_world_transformation(local_segment.end);
      windows.push_back(Segment{ start, end, SegmentLayer::Window });
    }
  }
}

void DRWParser::addBlock(const DRW_Block& data)
{
  if(m_verbose)
    std::println("[addBlock] name={}", data.name);
  if(data.name.contains("BLOCK-DOOR") || data.name.contains("BLOCK-WINDOW"))
  {
    m_current_block_name = data.name;
    m_is_parsing_block = true;
    m_block_vertices[m_current_block_name].clear();
  }
}

void DRWParser::endBlock()
{
  if(m_verbose)
    std::println("[endBlock] name={}", m_current_block_name);
  m_current_block_name.clear();
  m_is_parsing_block = false;
}
