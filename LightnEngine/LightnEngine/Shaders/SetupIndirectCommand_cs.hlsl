
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

StructuredBuffer<OutputInfo> objectDatas            : register(t0);
StructuredBuffer<IndirectCommand> inputCommands            : register(t1);    // SRV: Indirect commands
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
		uint numStructs = objectDatas[3072].id;
		//uint stride;
		//objectDatas.GetDimensions(numStructs, stride);

		//ˆê‚Â‚à•`‰æ‚³‚ê‚È‚¢ê‡‚Í•`‰æƒRƒ}ƒ“ƒhŽ©‘Ì‚ð’Ç‰Á‚µ‚È‚¢
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