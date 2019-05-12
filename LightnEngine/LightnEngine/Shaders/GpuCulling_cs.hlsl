#define ThreadBlockSize 256
#define FrustumPlaneCount 4

struct ObjectInfo {
	float4x4 mtxWorld;
	float3 startPosAABB;
	float3 endposAABB;
	float4 color;
	uint indirectArgumentIndex;
};

struct OutputInfo {
	float4x4 mtxWorld;
	float4 color;
};

cbuffer RootConstants : register(b0)
{
	float4 cameraPosition;
	float4 frustumPlanes[FrustumPlaneCount];//Right Left Top Bottom
};

StructuredBuffer<ObjectInfo> inputCommands            : register(t0);    // SRV: Indirect commands
AppendStructuredBuffer<OutputInfo> outputCommands[]    : register(u0);    // UAV: Processed indirect commands

[numthreads(ThreadBlockSize, 1, 1)]
void CSMain(uint3 groupId : SV_GroupID, uint3 threadGroupId : SV_GroupThreadID)
{
	uint index = groupId.x * ThreadBlockSize + threadGroupId.x;

	uint numStructure;
	uint stride;
	inputCommands.GetDimensions(numStructure, stride);

	if (index >= numStructure) {
		return;
	}

	ObjectInfo objectInfo = inputCommands[index];
	float3 viewPosition = objectInfo.startPosAABB - cameraPosition.xyz;
	uint inCount = 0;

	for (uint i = 0; i < FrustumPlaneCount; ++i) {
		float length = dot(frustumPlanes[i].xyz, viewPosition);

		if (length > 0) {
			inCount++;
		}
	}

	if (inCount == FrustumPlaneCount) {
		OutputInfo info;
		info.mtxWorld = objectInfo.mtxWorld;
		info.color = objectInfo.color;

		outputCommands[objectInfo.indirectArgumentIndex].Append(info);
	}
}