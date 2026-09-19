#include "mesh_visualizer.hpp"

#include "graphics/texture.hpp"
#include "graphics/static_mesh.hpp"
#include "graphics/transformation.hpp"

#include <glad/gl.h>
#include <GLFW/glfw3.h>

// ==============================
// Public
// ==============================

MeshVisualizer::MeshVisualizer(i32 width, i32 height) : win_width(width), win_height(height)
{
  auto aspect = static_cast<f32>(width) / static_cast<f32>(height);
  camera = Camera(0.1f, 500.0f, 45.f, aspect);

  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_DEPTH_BITS, 24);
  window = glfwCreateWindow(width, height, "Mesh visualizer", nullptr, nullptr);
  if(!window)
    throw std::runtime_error("Failed to create GLFW window");

  glfwMakeContextCurrent(window);
  auto version = gladLoadGL(glfwGetProcAddress);
  if(!version)
    throw std::runtime_error("Failed to initialize OpenGL context");

  glDisable(GL_CULL_FACE);
  glEnable(GL_DEPTH_TEST);    // enable depth testing
  glDepthFunc(GL_LESS);    	 // specify the value used for depth buffer comparisons
  glDepthMask(GL_TRUE);    	 // enable/disable writing into the depth buffer
  glClearDepth(1.0f);       // specify the clear value for the depth buffer
  glClearColor(0.15f, 0.30f, 0.45f, 1.0f);

  create_gl_pipeline_object();
}

MeshVisualizer::~MeshVisualizer()
{
  glfwDestroyWindow(window);
  glfwTerminate();
}

void MeshVisualizer::render(std::shared_ptr<StaticMesh> mesh, const std::vector<PrimitiveRange>& primitives)
{
  auto floor_texture = Texture::create_from_file("materials/patio_tiles/patio_tiles_diff_1k.jpg");
  auto wall_texture = Texture::create_from_file("materials/concrete_layers/concrete_layers_diff_1k.jpg");

  auto mesh_transform = Transformation{};
  while (!glfwWindowShouldClose(window))
  {
    auto width{ 0 }, height{ 0 };
    glfwGetFramebufferSize(window, &width, &height);
    auto aspect_ratio = static_cast<f32>(width) / static_cast<f32>(height);

    win_width = width;
    win_height = height;
    camera.aspect = aspect_ratio;

    glfwPollEvents();
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
      glfwSetWindowShouldClose(window, GLFW_TRUE);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glViewport(0, 0, win_width, win_height);

    handle_camera_input();

    auto mat_camera = camera.canonical_to_camera();
    auto mat_persp = camera.get_perspective();

    pipeline.bind();
    pipeline.set_active_program(vertex_program);
    vertex_program.set_uniform_mat4f(0, &mesh_transform.M[0][0]);
    vertex_program.set_uniform_mat4f(1, &mat_camera[0][0]);
    vertex_program.set_uniform_mat4f(2, &mat_persp[0][0]);

    mesh->vao.bind();
    for (const auto& prim : primitives)
    {
      if(prim.material == MaterialType::Floor)
        floor_texture.bind_texture_unit(0);
      else
        wall_texture.bind_texture_unit(0);

      glDrawElements(GL_TRIANGLES, prim.index_count, GL_UNSIGNED_INT, (void*)(uintptr_t)(prim.index_offset * sizeof(u32)));
    }


    glfwSwapBuffers(window);
  }
}

// ==============================
// Private
// ==============================

void MeshVisualizer::create_gl_pipeline_object()
{
  auto shaders_dir = std::filesystem::current_path() / "shaders";

  auto vertex_shader_obj = ShaderObject{};
  vertex_shader_obj.create(ShaderStage::Vertex);
  vertex_shader_obj.load_source_code(shaders_dir / "vertex_shader.glsl");
  vertex_shader_obj.compile();
  auto status = vertex_shader_obj.check_compile_status();
  if (!status)
    throw std::runtime_error(std::format("Shader compilation error: {}", vertex_shader_obj.get_compile_log()));

  auto fragment_shader_obj = ShaderObject{};
  fragment_shader_obj.create(ShaderStage::Fragment);
  fragment_shader_obj.load_source_code(shaders_dir / "fragment_shader.glsl");
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

void MeshVisualizer::handle_camera_input()
{
  constexpr auto velocity = 0.1f;

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
