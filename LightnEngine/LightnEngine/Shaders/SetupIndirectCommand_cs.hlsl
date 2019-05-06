
#define ThreadBlockSize 128

struct OutputInfo {
	float4x4 mtxWorld;
	float4 color;
};

cbuffer RootConstants : register(b0)
{
	uint totalLength;
};

struct IndirectCommand
{
	uint2 vbAddress;
	uint4 drawArguments;
	uint padding;
};

StructuredBuffer<OutputInfo> objectDatas            : register(t0);
StructuredBuffer<IndirectCommand> inputCommands            : register(t1);    // SRV: Indirect commands
AppendStructuredBuffer<IndirectCommand> outputCommands    : register(u0);    // UAV: Processed indirect commands

[numthreads(1, 1, 1)]
void CSMain(uint3 groupId : SV_GroupThreadID, uint groupIndex : SV_GroupThreadID)
{
	// Each thread of the CS operates on one of the indirect commands.
	uint index = (groupId.x * ThreadBlockSize) + groupIndex;
	uint totalLengthA = totalLength;
	totalLengthA = 1;
	index = 0;

	if (index < totalLengthA) {
		uint numStructs;
		uint stride;
		objectDatas.GetDimensions(numStructs, stride);

		//ˆê‚Â‚à•`‰æ‚³‚ê‚È‚¢ê‡‚Í•`‰æƒRƒ}ƒ“ƒhŽ©‘Ì‚ð’Ç‰Á‚µ‚È‚¢
		if (numStructs > 0) {
			IndirectCommand command;
			//command.vbAddress = inputCommands[index].vbAddress;
			command.drawArguments.x = 0;
			//command.drawArguments.y = inputCommands[index].drawArguments.y;
			command.drawArguments.z = numStructs;
			command.drawArguments.w = 0;
			command.padding = 0;

			//outputCommands.Append(command);
		}
	}
}