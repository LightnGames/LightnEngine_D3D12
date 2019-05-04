
#define threadBlockSize 128

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
	float xOffset;        // Half the width of the triangles.
	float zOffset;        // The z offset for the triangle vertices.
	float cullOffset;    // The culling plane offset in homogenous space.
	float commandCount;    // The number of commands to be processed.
};

StructuredBuffer<ObjectInfo> inputCommands            : register(t0);    // SRV: Indirect commands
AppendStructuredBuffer<OutputInfo> outputCommands    : register(u0);    // UAV: Processed indirect commands

[numthreads(threadBlockSize, 1, 1)]
void CSMain(uint3 groupId : SV_GroupID, uint groupIndex : SV_GroupIndex)
{
	// Each thread of the CS operates on one of the indirect commands.
	uint index = (groupId.x * threadBlockSize) + groupIndex;

	if (inputCommands[index].startPosAABB.x > xOffset) {
		OutputInfo info;
		info.mtxWorld = inputCommands[index].mtxWorld;
		info.color = inputCommands[index].color;
		outputCommands.Append(info);
	}
}