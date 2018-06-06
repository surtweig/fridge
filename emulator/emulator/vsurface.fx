//--------------------------------------------------------------------------------------
// Constant Buffer Variables
//--------------------------------------------------------------------------------------
Texture2D vbuffer : register( t0 );
SamplerState vsampler : register( s0 );

/*cbuffer cbTransform : register( b0 )
{
    matrix View;
	matrix World;
	matrix Projection;
};*/

cbuffer cbPalette : register( b0 )
{
	float4 palette[16];
};


//--------------------------------------------------------------------------------------
struct VS_INPUT
{
    float4 Pos : POSITION;
    float2 Tex : TEXCOORD0;
	float2 Color : COLOR;
};

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float2 Tex : TEXCOORD0;
	float2 Color : COLOR;
};


//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------
PS_INPUT VS( VS_INPUT input )
{
    PS_INPUT output = (PS_INPUT)0;
	output.Pos = input.Pos;
    output.Tex = input.Tex;
	output.Color = input.Color;
    
    return output;
}


//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------
float4 PS( PS_INPUT input ) : SV_Target
{
	float ci = vbuffer.Sample(vsampler, input.Tex).r;

	// For normal (raster) mode input.Color is always zero, so ci is left unchanged
	// For text mode input.Color.g and .r encode background and foreground color respectively
	ci = lerp(ci + input.Color.g, ci - input.Color.r, ci);
	//ci = lerp(input.Color.g, input.Color.r, ci);

	return palette[(int)(min(ci, 0.99) * 16.0)];
	//return palette[3];
	return float4(ci, ci, ci, 1.0);
}
