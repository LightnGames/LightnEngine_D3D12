
#define ThreadBlockSize 128
#define FrustumPlaneCount 4

struct ObjectInfo {
	float4x4 mtxWorld;
	float3 startPosAABB;
	float3 endposAABB;
	float4 color;
};

struct OutputInfo {
	float4x4 mtxWorld;
	float4 color;
};

cbuffer RootConstants : register(b0)
{
	float4 cameraPosition;
	float4 frustumPlanes[FrustumPlaneCount];//Right Left Top Bottom Near Far[-Near]
};

StructuredBuffer<ObjectInfo> inputCommands            : register(t0);    // SRV: Indirect commands
AppendStructuredBuffer<OutputInfo> outputCommands    : register(u0);    // UAV: Processed indirect commands

[numthreads(ThreadBlockSize, 1, 1)]
void CSMain(uint3 groupId : SV_GroupID, uint groupIndex : SV_GroupIndex)
{
	// Each thread of the CS operates on one of the indirect commands.
	uint index = (groupId.x * ThreadBlockSize) + groupIndex;

	float3 viewPosition = inputCommands[index].startPosAABB - cameraPosition.xyz;
	uint inCount = 0;
	for (uint i = 0; i < FrustumPlaneCount; ++i) {
		float length = dot(frustumPlanes[i].xyz, viewPosition);
		if (length > 0) {
			inCount++;
		}
	}

	if (inCount == FrustumPlaneCount) {
		OutputInfo info;
		info.mtxWorld = inputCommands[index].mtxWorld;
		info.color = inputCommands[index].color;
		outputCommands.Append(info);
		return;
	}

	OutputInfo info;
	info.mtxWorld = float4x4(
		0,0,0,0,
		0,0,0,0,
		0,0,0,0,
		0,0,0,0);
	info.color = inputCommands[index].color;
	outputCommands.Append(info);
}