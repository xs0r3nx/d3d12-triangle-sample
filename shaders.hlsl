//Inputs for Pixel shader that are recieved from vertex buffer
struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};
//vertex shader
//setting the results of the vertex shader as our pixel shader input
PSInput VSMain(float4 position : POSITION, float4 color : COLOR)
{
    PSInput result;

    result.position = position;
    result.color = color;

    return result;
}
//pixel shader
//giving it the pixel shader input
float4 PSMain(PSInput input) : SV_TARGET
{
    return input.color;
}
