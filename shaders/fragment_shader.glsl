#version 460 core

layout(location = 0) in vec3 vs_out_normal_world_space;
layout(location = 1) in vec2 vs_out_text_coord;
layout(location = 2) in vec3 vs_out_frag_world_space;

layout(location = 0) out vec4 fs_out_color;

layout(binding = 0) uniform sampler2D u_texture_color;

const vec3 LIGHT_POS = vec3(0.0, 1.0, 0.0);
const float LIGHT_POWER = 10.0;
const float PI = 3.14159265359;

void main()
{
  vec3 surface_color = texture(u_texture_color, vs_out_text_coord).rgb;
  surface_color = pow(surface_color, vec3(2.2));

  vec3 N = normalize(vs_out_normal_world_space);
  vec3 L = LIGHT_POS - vs_out_frag_world_space;
  float distance = length(L);
  L = normalize(L);

  float irradiance = LIGHT_POWER / (4.0 * PI * distance * distance);
  float NdotL = max(dot(N, L), 0.0);

  vec3 diffuse = (surface_color / PI) * irradiance * NdotL;
  vec3 ambient = 0.05 * surface_color;
  vec3 result = ambient + diffuse;

  result = pow(result, vec3(1.0 / 2.2));
  fs_out_color = vec4(result, 1.0);
}
