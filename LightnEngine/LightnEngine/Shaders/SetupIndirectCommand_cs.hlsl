
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
	uint vbSizeInBytes;
	uint vbStrideInBytes;

	uint2 ibAddress;
	uint ibSizeInBytes;
	uint ibFormat;
	
	uint2 perInstanceVbAddress;
	uint perInstanceSizeInBytes;
	uint perInstanceStrideInBytes;
	
	uint4 drawArguments;
	uint2 padding;
};

StructuredBuffer<IndirectCommand> inputCommands            : register(t0);    // SRV: Indirect commands
StructuredBuffer<OutputInfo> objectDatas[]            : register(t1);
AppendStructuredBuffer<IndirectCommand> outputCommands    : register(u0);    // UAV: Processed indirect commands

[numthreads(1, 1, 1)]
void CSMain(uint3 groupId : SV_GroupID)
{	
	uint argumentIndex = groupId.x;
	//uint totalLengthA = totalLength;
	//totalLengthA = 1;
	//index = 0;

	uint numStructs = objectDatas[argumentIndex][3072].id;//AppendStructuredBufferのカウントに直接アクセス

	//一つも描画されない場合は描画コマンド自体を追加しない
	if (numStructs > 0) {
		IndirectCommand command = inputCommands[argumentIndex];
		command.drawArguments.y = numStructs;
		command.drawArguments.z = 0;
		command.drawArguments.w = 0;
		command.padding = 0;

		outputCommands.Append(command);
	}
}