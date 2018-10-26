#version 150

in vec3 vertexWC;
in float vertexData;

out vec4 colorVSOut;

uniform mat4 MCDCMatrix;
uniform sampler2D colorTex;
uniform vec4 solidColor;
uniform float opacity;
uniform vec2 dataRange;
uniform int hasVertexData;

void main()
{
  gl_Position = MCDCMatrix * vec4(vertexWC, 1.0);
  vec4 color = solidColor;
  if (hasVertexData == 1)
  {
    //color = texture2D(colorTex, vec2(vertexData - dataRange.x, 0.0));
    color = texture(colorTex, vec2((vertexData - dataRange.x)/(dataRange.y - dataRange.x), 0.0));
  }
  colorVSOut = vec4(color.rgb, color.a * opacity);
}
