cbuffer ConstantBuffer : register(b0) {
    float4 buttonColor;
};

struct VS_INPUT {
    float3 Pos : POSITION;
};

struct PS_INPUT {
    float4 Pos : SV_POSITION;
};

PS_INPUT VS(VS_INPUT input) {
    PS_INPUT output;
    output.Pos = float4(input.Pos, 1.0f);
    return output;
}

float4 PS(PS_INPUT input) : SV_Target {
    return buttonColor;
}