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

StructuredBuffer<InIndirectCommand> inputCommands : register(t0);//元の描画コマンド　メッシュインデックス付き
StructuredBuffer<uint> countBufferOffsets : register(t1);//AppendStructureBufferのカウント値までのオフセット数
ByteAddressBuffer objectDatas[] : register(t2);//描画されるインスタシング用の行列が入っている。今回はカウント値を見るだけ
AppendStructuredBuffer<IndirectCommand> outputCommands : register(u0);//ExecuteIndirectに渡すバッファ

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