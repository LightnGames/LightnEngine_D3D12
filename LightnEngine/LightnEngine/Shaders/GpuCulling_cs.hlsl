#define ThreadBlockSize 256
#define FrustumPlaneCount 4

struct AABB {
	float3 min;
	float3 max;
};

struct ObjectInfo {
	float4x4 mtxWorld;
	AABB boundingBox;
	float4 color;
	uint indirectArgumentIndex;
};

struct OutputInfo {
	float4x4 mtxWorld;
	float4 color;
};

cbuffer RootConstants : register(b0)
{
	float4 cameraPosition;
	float4 frustumPlanes[FrustumPlaneCount];//Right Left Top Bottom
};

//AABB�̖@�������ɍł��߂��|�C���g��T��
float3 GetPositivePoint(AABB aabb, float3 planeNormal)
{
	float3 result = aabb.min;
	float3 size = aabb.max - aabb.min;

	result.x += lerp(size.x, 0, (float)(planeNormal.x > 0 ? 0 : 1));
	result.y += lerp(size.y, 0, (float)(planeNormal.y > 0 ? 0 : 1));
	result.z += lerp(size.z, 0, (float)(planeNormal.z > 0 ? 0 : 1));

	return result;
}

StructuredBuffer<ObjectInfo> inputCommands            : register(t0);    // SRV: Indirect commands
AppendStructuredBuffer<OutputInfo> outputCommands[]    : register(u0);    // UAV: Processed indirect commands

[numthreads(ThreadBlockSize, 1, 1)]
void CSMain(uint3 groupId : SV_GroupID, uint3 threadGroupId : SV_GroupThreadID)
{
	uint index = groupId.x * ThreadBlockSize + threadGroupId.x;

	uint numStructure;
	uint stride;
	inputCommands.GetDimensions(numStructure, stride);

	//256�X���b�h�P�ʂœ������Ă���̂ň�ꂽ�牽�����Ȃ�
	if (index >= numStructure) {
		return;
	}

	ObjectInfo objectInfo = inputCommands[index];
	uint inCount = 0;

	//�����䕽�ʂ̐������`�F�b�N
	for (uint i = 0; i < FrustumPlaneCount; ++i) {
		float3 aabbPos = GetPositivePoint(objectInfo.boundingBox, frustumPlanes[i].xyz);
		float3 viewPosition = aabbPos - cameraPosition.xyz;
		float length = dot(frustumPlanes[i].xyz, viewPosition);

		//���ʂ�AABB�̕��ʕ����̊p�������ɂ���Γ��ς̒l���v���X�ɂȂ�
		if (length > 0) {
			inCount++;
		}
	}

	//�����䂷�ׂĂ̕��ʓ��ɓ����Ă�����`�惊�X�g�ɉ�����
	if (inCount == FrustumPlaneCount) {
		OutputInfo info;
		info.mtxWorld = objectInfo.mtxWorld;
		info.color = objectInfo.color;

		outputCommands[objectInfo.indirectArgumentIndex].Append(info);
	}
}