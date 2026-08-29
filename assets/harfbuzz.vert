
/*
 * Copyright 2026 Behdad Esfahbod. All Rights Reserved.
 */

uniform mat4 u_matViewProjection;
uniform vec2 u_viewport;
uniform float u_scale;
uniform vec2 u_position;

in vec2 a_position;
in vec2 a_texcoord;
in vec2 a_normal;
in float a_emPerPos;
in uint a_glyphLoc;
in uint a_hasCursor;

out vec2 v_texcoord;
flat out uint v_hasCursor;
flat out uint v_glyphLoc;

void main()
{
   vec2 pos    = a_position;
   vec2 tex    = a_texcoord;
   float scale = u_scale;
   float epp   = a_emPerPos;

   epp /= scale;
   pos = (pos + tex) * scale;
   pos += u_position;

   vec4 jac = vec4(epp, 0.0, 0.0, -epp);
   hb_gpu_dilate(pos, tex, a_normal, jac, u_matViewProjection, u_viewport);

   gl_Position = u_matViewProjection * vec4(pos, 0.0, 1.0);
   v_texcoord  = tex;
   v_glyphLoc  = a_glyphLoc;
   v_hasCursor = a_hasCursor;
}
