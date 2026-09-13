#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <exception>
#include <filesystem>
#include <print>
#include <format>
#include <memory>
#include <stdexcept>
#include <atomic>

#include "dump.hpp"
#include "misc.hpp"
#include "types.hpp"
#include "reconstruction.hpp"
#include "graphics/texture.hpp"
#include "graphics/pipeline.hpp"
#include "graphics/camera.hpp"
#include "graphics/static_mesh.hpp"
#include "graphics/transformation.hpp"
#include "graphics/framebuffer.hpp"
#include "io/gltf_exporter.hpp"
#include "io/config_loader.hpp"
#include "gui/gui.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <glm/trigonometric.hpp>
#include <glm/geometric.hpp>
#include <vector>

// static constexpr auto initial_window_width = 1280;
// static constexpr auto initial_window_height = 720; // aspect ratio 16:9
// static auto viewport_info = ViewportInfo{
//   .width=initial_window_width,
//   .height=initial_window_height,
//   .screen_pos=glm::vec2{0, 0},
//   .aspect=static_cast<f32>(initial_window_width) / static_cast<f32>(initial_window_height)
// };

// static auto vertex_program = ShaderProgram{};
// static auto fragment_program = ShaderProgram{};
// static auto pipeline = ProgramPipelineObject{};

// static auto fbo = FrameBuffer{};
// static auto fbo_color_texture = Texture{};
// static auto fbo_depth_texture = Texture{};

// static auto light_position = glm::vec3{ 0.0f, 2.0f, 0.0f };
// static auto light_power = 700.0f; // in watt

// static auto camera = Camera(0.1f, 500.0f, 45.f, viewport_info.aspect);
// static constexpr auto camera_speed = 0.1f;

// static auto static_mesh = std::unique_ptr<StaticMesh>{};
// static auto mesh_transform = Transformation{};

// static auto viewport_image = Texture{};
// static auto plot_image = Texture{};

// static auto current_stage = ReconstructionStage::PrimitivesExtraction;
// static auto worker_state = std::atomic<ThreadState>{ ThreadState::Idle };
// static auto worker = std::optional<std::jthread>{};
// static auto worker_is_done = std::atomic<bool>{ false };
// static auto build_result = ReconstructionResult{};


#if 0
static void create_gl_pipeline_object(ShaderProgram& vertex_program, ShaderProgram& fragment_program, ProgramPipelineObject& pipeline)
{
  auto shaders_dir = std::filesystem::current_path() / "shaders";

  auto vertex_shader_obj = ShaderObject{};
  vertex_shader_obj.create(ShaderStage::Vertex);
  vertex_shader_obj.load_source_code(shaders_dir / "basic_shader.vert.glsl");
  vertex_shader_obj.compile();
  auto status = vertex_shader_obj.check_compile_status();
  if (!status)
    throw std::runtime_error(std::format("Shader compilation error: {}", vertex_shader_obj.get_compile_log()));

  auto fragment_shader_obj = ShaderObject{};
  fragment_shader_obj.create(ShaderStage::Fragment);
  fragment_shader_obj.load_source_code(shaders_dir / "basic_shader.frag.glsl");
  fragment_shader_obj.compile();
  status = fragment_shader_obj.check_compile_status();
  if (!status)
    throw std::runtime_error(std::format("Shader compilation error: {}", fragment_shader_obj.get_compile_log()));

  vertex_program.create();
  vertex_program.attach_shader(vertex_shader_obj);
  vertex_program.set_separable(true);
  vertex_program.link();
  status = vertex_program.check_link_status();
  if (!status)
    throw std::runtime_error(std::format("Link status: {}", vertex_program.get_link_log()));

  vertex_program.detach_shader(vertex_shader_obj);

  fragment_program.create();
  fragment_program.attach_shader(fragment_shader_obj);
  fragment_program.set_separable(true);
  fragment_program.link();
  status = fragment_program.check_link_status();
  if (!status)
    throw std::runtime_error(std::format("Link status: {}", fragment_program.get_link_log()));

  fragment_program.detach_shader(fragment_shader_obj);

  pipeline.create();
  pipeline.bind_program_stage(PipelineStage::VertexShader, vertex_program);
  pipeline.bind_program_stage(PipelineStage::FragmentShader, fragment_program);
  status = pipeline.validate_pipeline();
  if (!status)
    throw std::runtime_error(std::format("pipeline object status: {}", pipeline.get_validation_status()));
}

static void handle_camera_input(GLFWwindow* window, Camera& camera)
{
  constexpr auto velocity = camera_speed;

  if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)    camera.rotate_pitch(+glm::radians(1.0f));
  if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)  camera.rotate_pitch(-glm::radians(1.0f));
  if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)  camera.rotate_yaw(+glm::radians(1.0f));
  if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) camera.rotate_yaw(-glm::radians(1.0f));
  if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)     camera.eye += camera.gaze() * velocity;
  if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)     camera.eye -= camera.gaze() * velocity;
  if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)     camera.eye -= camera.right() * velocity;
  if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)     camera.eye += camera.right() * velocity;
  if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) camera.eye += camera.up() * velocity;
  if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) camera.eye -= camera.up() * velocity;
}

static void create_framebuffer(FrameBuffer& fb, Texture& color, Texture& depth, i32 width, i32 height)
{
  // --- Color texture ---
  color.create(TextureType::Texture2D);
  color.set_storage_tex2D(1, TextureImageFormat::RGBA8, width, height);
  color.set_wrap_mode(TextureWrapMode::ClampToEdge, TextureWrapMode::ClampToEdge);
  color.set_magnification_filter(TextureFilteringMode::Linear);
  color.set_minification_filter(TextureFilteringMode::Linear);

  // --- Depth texture  ---
  depth.create(TextureType::Texture2D);
  depth.set_storage_tex2D(1, TextureImageFormat::Depth24Stencil8, width, height);
  depth.set_wrap_mode(TextureWrapMode::ClampToEdge, TextureWrapMode::ClampToEdge);
  depth.set_magnification_filter(TextureFilteringMode::Nearest);
  depth.set_minification_filter(TextureFilteringMode::Nearest);

  fb.create();
  fb.bind(FramebufferTarget::READ_DRAW);
  fb.attach_texture(FramebufferAttachment::COLOR_0, color, 0);
  fb.attach_texture(FramebufferAttachment::DEPTH_STENCIL, depth, 0);

  if (!fb.check_status())
    throw std::runtime_error("Invalid framebuffer object!");

  fb.unbind(FramebufferTarget::READ_DRAW);
}

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
#endif

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
    doors_reconstruction(parsing_result.doors, hash, edges, 0);
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
    auto window_width = config.window_width;

    // Sample points from window segments and calculate clusters
    auto sample_points = sample_segments(windows, num_samples);
    auto clusters = calculate_clusters(sample_points, eps);
    std::println("sample_points: {}, clusters: {}", sample_points.size(), clusters.size());
    dump_clusters_csv(sample_points, clusters, "out/clusters.csv");
    output_name = std::format("out/clusters_{:04d}.jpg", counter++);
    run_python_script("plot_clusters.py", output_name);
    std::filesystem::remove("out/clusters.csv");

    windows_reconstruction(sample_points, clusters, hash, edges, 0);
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

  

#if 0
  auto window_context = init_window_context(viewport_info.width, viewport_info.height);

  create_gl_pipeline_object(vertex_program, fragment_program, pipeline);

  camera.eye = { 0.0f, 2.0f, 8.0f };
  // camera.set_orientation(glm::radians(glm::vec3{ 170.f, 40.f, 180.f }));

  auto floor_texture = Texture::create_from_file("materials/patio_tiles/patio_tiles_diff_1k.jpg");
  auto wall_texture = Texture::create_from_file("materials/concrete_layers/concrete_layers_diff_1k.jpg");

  while (!glfwWindowShouldClose(window_context))
  {
    glfwPollEvents();
    if (glfwGetKey(window_context, GLFW_KEY_ESCAPE) == GLFW_PRESS)
      glfwSetWindowShouldClose(window_context, GLFW_TRUE);

    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    setup_docking();

    // =======================================================
    // Start worker if idle
    // =======================================================
    if(worker_state == ThreadState::Idle)
    {
      switch (current_stage)
      {
        // =======================================================
        // Mesh building
        // =======================================================
        case ReconstructionStage::BuildMesh:
        {
          g_logger.push_message({"[worker] Starting BuildMesh...", LogLevel::Info});
          worker_state = ThreadState::Running;
          worker.emplace([&] {
            try
            {
              // remove all FLOOR faces and push only one quad for floor
              std::erase_if(ctx.faces, [](auto face) { return face.type == FaceType::Floor; });
              auto house_bbox = BoundingBox2D(ctx.walls);
              auto floor_face = create_floor_face(house_bbox);
              ctx.faces.push_back(std::move(floor_face));
              build_result = Reconstruction::build_mesh(ctx.faces);
              worker_is_done = true;
            }
            catch (const std::exception& e)
            {
              g_logger.push_message({std::format("[worker] Error during BuildMesh.\n{}", e.what()), LogLevel::Error});
              worker_state = ThreadState::Error;
              worker_is_done = true;
            }
          });
          break;
        }

        default: break;
      }
    }

    // =======================================================
    // Worker completion (Running -> WaitingConfirmation)
    // =======================================================
    else if(worker_state == ThreadState::Running && worker_is_done.load())
    {
      worker_is_done = false;
      switch (current_stage)
      {
        // ===========================================
        // On BuildMesh completed
        // ===========================================
        case ReconstructionStage::BuildMesh:
        {
          g_logger.push_message({"BuildMesh completed! Creating static_mesh...", LogLevel::Success});
          static_mesh = std::make_unique<StaticMesh>(build_result.mesh_vertices.data(),
                                                    build_result.mesh_vertices.size(),
                                                    build_result.mesh_indices.data(),
                                                    build_result.mesh_indices.size());

          // e.g. out/draftperson_Floor_Plan.gltf
          auto gltf_path = std::filesystem::path("out") / g_config.dxf_path.filename().replace_extension("gltf");
          export_to_gltf(build_result, gltf_path);
          g_logger.push_message({std::format("The exported model: {}", gltf_path.string()), LogLevel::Text});

          // e.g. out/draftperson_Floor_Plan_openings.json
          auto json_filename = g_config.dxf_path.stem().string() + "_openings.json";
          auto json_path = std::filesystem::path("out") / json_filename;
          export_opening_placeholders(build_result, json_path);
          g_logger.push_message({std::format("The exported placeholder: {}", json_path.string()), LogLevel::Text});
          break;
        }

        default:
          break;
      }

    }

    // =======================================================
    // Viewport panel
    // =======================================================
    auto flip_viewport_image = (current_stage == ReconstructionStage::RenderMesh && static_mesh);
    auto new_info = viewport_panel(viewport_image, flip_viewport_image);
    if(viewport_info.width != new_info.width || viewport_info.height != new_info.height)
    {
      viewport_info = new_info;
      camera.aspect = viewport_info.aspect;
      if(current_stage == ReconstructionStage::RenderMesh && fbo.is_valid())
      {
        fbo.destroy();
        fbo_color_texture.destroy();
        fbo_depth_texture.destroy();
        create_framebuffer(fbo, fbo_color_texture, fbo_depth_texture, viewport_info.width, viewport_info.height);
      }
    }

    // =======================================================
    // Model rendering
    // =======================================================
    if(current_stage == ReconstructionStage::RenderMesh && static_mesh)
    {
      if(!fbo.is_valid())
        create_framebuffer(fbo, fbo_color_texture, fbo_depth_texture, viewport_info.width, viewport_info.height);

      fbo.bind(FramebufferTarget::READ_DRAW);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      glViewport(0, 0, viewport_info.width, viewport_info.height);

      handle_camera_input(window_context, camera);
      auto mat_camera = camera.canonical_to_camera();
      auto mat_persp = camera.get_perspective();

      pipeline.bind();
      pipeline.set_active_program(vertex_program);
      vertex_program.set_uniform_mat4f(0, &mesh_transform.M[0][0]);
      vertex_program.set_uniform_mat4f(1, &mat_camera[0][0]);
      vertex_program.set_uniform_mat4f(2, &mat_persp[0][0]);
      pipeline.set_active_program(fragment_program);
      fragment_program.set_uniform_vector3f(0, &light_position[0]);
      fragment_program.set_uniform_f32(1, light_power);

      static_mesh->vao.bind();
      for (const auto& prim : build_result.primitives)
      {
        if(prim.material == MaterialType::Floor)
          floor_texture.bind_texture_unit(0);
        else
          wall_texture.bind_texture_unit(0);

        glDrawElements(GL_TRIANGLES, prim.index_count, GL_UNSIGNED_INT, (void*)(uintptr_t)(prim.index_offset * sizeof(u32)));
      }

      fbo.unbind(FramebufferTarget::READ_DRAW);

      viewport_image = fbo_color_texture;

      mesh_details_overlay(*static_mesh, new_info.screen_pos);
    }

    // =======================================================
    // Log panel
    // =======================================================
    console_panel(window_context, current_stage, worker_state);

    // =======================================================
    // Properties, Scene panels
    // =======================================================
    properties_panel();
    scene_panel(camera, mesh_transform, light_position, light_power);

    render_gui();
    glfwSwapBuffers(window_context);
  }
  glfwTerminate();

  if(plot_image.is_valid()) plot_image.destroy();
  if(fbo.is_valid())
  {
    fbo.destroy();
    fbo_color_texture.destroy();
    fbo_depth_texture.destroy();
  }
  return 0;
#endif
}
