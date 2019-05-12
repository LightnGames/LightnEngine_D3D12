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
	
	uint drawArguments[6];
};

StructuredBuffer<IndirectCommand> inputCommands : register(t0);    // SRV: Indirect commands
StructuredBuffer<uint> countBufferOffsets : register(t1);
ByteAddressBuffer objectDatas[] : register(t2);
AppendStructuredBuffer<IndirectCommand> outputCommands : register(u0);    // UAV: Processed indirect commands

[numthreads(1, 1, 1)]
void CSMain(uint3 groupId : SV_GroupID)
{	
	uint argumentIndex = groupId.x;
	uint offsetCounterNum = countBufferOffsets[argumentIndex];
	uint numStructs = objectDatas[argumentIndex].Load(offsetCounterNum);//AppendStructuredBufferのカウントに直接アクセス

	//一つも描画されない場合は描画コマンド自体を追加しない
	if (numStructs > 0) {
		IndirectCommand command = inputCommands[argumentIndex];
		command.drawArguments[1] = numStructs;
		command.drawArguments[2] = 0;
		command.drawArguments[3] = 0;
		command.drawArguments[4] = 0;
		command.drawArguments[5] = 0;

		outputCommands.Append(command);
	}
}