using UnrealBuildTool;
using System.Collections.Generic;

public class SurvivalGameTarget : TargetRules
{
	public SurvivalGameTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;

		ExtraModuleNames.AddRange( new string[] { "SurvivalGame" } );
	}
}
