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
	
	uint4 textureIndices;
	uint drawArguments[6];
};

struct InIndirectCommand {
	uint4 meshIndex;
	IndirectCommand indirectCommand;
};

StructuredBuffer<InIndirectCommand> inputCommands : register(t0);    // SRV: Indirect commands
StructuredBuffer<uint> countBufferOffsets : register(t1);
ByteAddressBuffer objectDatas[] : register(t2);
AppendStructuredBuffer<IndirectCommand> outputCommands : register(u0);    // UAV: Processed indirect commands

[numthreads(1, 1, 1)]
void CSMain(uint3 groupId : SV_GroupID)
{	
	uint argumentIndex = groupId.x;
	uint meshIndex = inputCommands[argumentIndex].meshIndex.x;
	uint offsetCounterNum = countBufferOffsets[meshIndex];
	uint numStructs = objectDatas[meshIndex].Load(offsetCounterNum);//AppendStructuredBufferのカウントに直接アクセス

	//一つも描画されない場合は描画コマンド自体を追加しない
	if (numStructs > 0) {
		IndirectCommand command = inputCommands[argumentIndex].indirectCommand;
		command.drawArguments[1] = numStructs;

		outputCommands.Append(command);
	}
}