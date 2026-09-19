#pragma once

#include "types.hpp"

#include "graphics/camera.hpp"
#include "graphics/pipeline.hpp"

#include <memory>

class MeshVisualizer
{
public:
  MeshVisualizer(i32 width, i32 height);
  ~MeshVisualizer();

  void render(std::shared_ptr<class StaticMesh> mesh, const std::vector<struct PrimitiveRange>& primitives);

  i32 win_width, win_height;
  struct GLFWwindow* window;
  Camera camera;

  ShaderProgram vertex_program;
  ShaderProgram fragment_program;
  ProgramPipelineObject pipeline;

private:
  void create_gl_pipeline_object();
  void handle_camera_input();
};
