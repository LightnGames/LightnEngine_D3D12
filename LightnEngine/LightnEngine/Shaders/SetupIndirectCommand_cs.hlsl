
#define ThreadBlockSize 128
#define FrustumPlaneCount 4

struct OutputInfo {
	uint id;
	//float4x4 mtxWorld;
	//float4 color;
};

//cbuffer RootConstants : register(b0)
//{
//	//uint totalLength;
//	float4 cameraPosition;
//	float4 frustumPlanes[FrustumPlaneCount];//Right Left Top Bottom Near Far[-Near]
//};

struct IndirectCommand
{
	uint2 vbAddress;
	uint sizeInBytes;
	uint strideInBytes;
	uint4 drawArguments;
	uint2 padding;
};

StructuredBuffer<IndirectCommand> inputCommands            : register(t0);    // SRV: Indirect commands
StructuredBuffer<OutputInfo> objectDatas[]            : register(t1);
AppendStructuredBuffer<IndirectCommand> outputCommands    : register(u0);    // UAV: Processed indirect commands

[numthreads(ThreadBlockSize, 1, 1)]
void CSMain(uint3 groupId : SV_GroupThreadID, uint groupIndex : SV_GroupThreadID)
{	
	// Each thread of the CS operates on one of the indirect commands.
	uint index = (groupId.x * ThreadBlockSize) + groupIndex;
	//uint totalLengthA = totalLength;
	//totalLengthA = 1;
	//index = 0;

	if (index == 0) {
		uint numStructs = objectDatas[0][3072].id;//AppendStructuredBufferのカウントに直接アクセス

		//一つも描画されない場合は描画コマンド自体を追加しない
		if (numStructs > 0) {
			IndirectCommand command = (IndirectCommand)0;
			command.vbAddress = inputCommands[index].vbAddress;
			command.sizeInBytes = inputCommands[index].sizeInBytes;
			command.strideInBytes = inputCommands[index].strideInBytes;
			command.drawArguments.x = inputCommands[index].drawArguments.x;
			command.drawArguments.y = numStructs;
			command.drawArguments.z = 0;
			command.drawArguments.w = 0;
			command.padding = 0;

			outputCommands.Append(command);
		}
	}
}