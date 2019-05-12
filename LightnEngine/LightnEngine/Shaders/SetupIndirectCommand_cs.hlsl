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
		command.drawArguments.y = numStructs;
		command.drawArguments.z = 0;
		command.drawArguments.w = 0;
		command.padding = 0;

		outputCommands.Append(command);
	}
}