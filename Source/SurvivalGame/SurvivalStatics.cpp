#include "SurvivalStatics.h"

UClass* USurvivalStatics::GetObjectParentClass(UObject* Object)
{
	return Object->GetClass()->GetSuperClass();
}
