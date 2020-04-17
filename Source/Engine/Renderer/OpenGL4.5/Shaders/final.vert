/*

Angie Engine Source Code

MIT License

Copyright (C) 2017-2020 Alexander Samusev.

This file is part of the Angie Engine Source Code.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

#include "viewuniforms.glsl"

out gl_PerVertex
{
	vec4 gl_Position;
};

layout( location = 0 ) noperspective out vec4 VS_TexCoord;
layout( location = 1 ) flat out float VS_Exposure;

layout( binding = 6 ) uniform sampler2D luminanceTexture;

void main() {
	gl_Position = vec4( InPosition, 0.0, 1.0 );
	VS_TexCoord.xy = InPosition * 0.5 + 0.5;
	VS_TexCoord.xy *= GetDynamicResolutionRatio();
	VS_TexCoord.y = 1.0 - VS_TexCoord.y;
	VS_TexCoord.zw = InPosition * vec2(0.5,-0.5) + 0.5;
	VS_Exposure = texelFetch( luminanceTexture, ivec2( 0 ), 0 ).x;
	VS_Exposure = GetPostprocessExposure() / VS_Exposure;
	//!!!   VS_Exposure = GetPostprocessExposure() / pow(VS_Exposure,2);
}
