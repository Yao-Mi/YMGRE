#include "../OPOBJ/YMGRE_OBJ.h"
#include "../CONFIG/YMGRE_Mem.h"
#include "../CONFIG/YMGRE_PubDefine.h"
#include "./YMGRE_List.h"
#include <string.h>

GRE_Material YMGRE_Material_Find(GRE_List materialList, char* materialName)
{
	if (materialList == NULL || materialName == NULL)
		return NULL;
	uint32 nameLen = (uint32)strlen(materialName) + 1;
	for (GRE_ListNode node = materialList->listhead; node != NULL; node = node->next)
	{
		GRE_Material material = node->data;
		if (material != NULL && material->nameLen == nameLen &&
			YMGRE_Memcmp(material->name, materialName, nameLen) == 0)
			return material;
	}
	return NULL;
}
