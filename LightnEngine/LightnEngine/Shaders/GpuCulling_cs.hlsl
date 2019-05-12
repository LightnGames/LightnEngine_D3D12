
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

StructuredBuffer<ObjectInfo> inputCommands[]            : register(t0);    // SRV: Indirect commands
AppendStructuredBuffer<OutputInfo> outputCommands[]    : register(u0);    // UAV: Processed indirect commands

[numthreads(ThreadBlockSize, 1, 1)]
void CSMain(uint3 groupId : SV_GroupID, uint3 threadGroupId : SV_GroupThreadID)
{
	// Each thread of the CS operates on one of the indirect commands.
	uint meshIndex = groupId.x;
	uint index = threadGroupId.x;

	uint numStructure;
	uint stride;
	inputCommands[meshIndex].GetDimensions(numStructure, stride);

	if (index >= numStructure) {
		return;
	}

	ObjectInfo objectInfo = inputCommands[meshIndex][index];
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

		outputCommands[meshIndex].Append(info);
	}
}