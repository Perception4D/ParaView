#version 150

in vec4 colorVSOut;
out vec4 fragColor;

void main()
{
  fragColor = colorVSOut;
}
