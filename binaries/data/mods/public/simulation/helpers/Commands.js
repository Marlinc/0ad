function ProcessCommand(player, cmd)
{
	// Do some basic checks here that commanding player is valid
	var cmpPlayerMan = Engine.QueryInterface(SYSTEM_ENTITY, IID_PlayerManager);
	if (!cmpPlayerMan || player < 0)
		return;
	var playerEnt = cmpPlayerMan.GetPlayerByID(player);
	if (playerEnt == INVALID_ENTITY)
		return;
	var cmpPlayer = Engine.QueryInterface(playerEnt, IID_Player);
	if (!cmpPlayer)
		return;
	var controlAllUnits = cmpPlayer.CanControlAllUnits();

	// Now handle various commands
	switch (cmd.type)
	{
	case "debug-print":
		print(cmd.message);
		break;

	case "chat":
		var cmpGuiInterface = Engine.QueryInterface(SYSTEM_ENTITY, IID_GuiInterface);
		cmpGuiInterface.PushNotification({"type": "chat", "player": player, "message": cmd.message});
		break;

	case "control-all":
		cmpPlayer.SetControlAllUnits(cmd.flag);
		break;
		
	case "reveal-map":
		// Reveal the map for all players, not just the current player,
		// primarily to make it obvious to everyone that the player is cheating
		var cmpRangeManager = Engine.QueryInterface(SYSTEM_ENTITY, IID_RangeManager);
		cmpRangeManager.SetLosRevealAll(-1, cmd.enable);
		break;

	case "walk":
		var entities = FilterEntityList(cmd.entities, player, controlAllUnits);
		var cmpUnitAI = GetFormationUnitAI(entities);
		if (cmpUnitAI)
			cmpUnitAI.Walk(cmd.x, cmd.z, cmd.queued);
		break;

	case "attack":
		// Check if target is owned by player's enemy
		if (IsOwnedByEnemyOfPlayer(player, cmd.target))
		{
			var entities = FilterEntityList(cmd.entities, player, controlAllUnits);
			var cmpUnitAI = GetFormationUnitAI(entities);
			if (cmpUnitAI)
				cmpUnitAI.Attack(cmd.target, cmd.queued);
		}
		break;

	case "repair":
		// This covers both repairing damaged buildings, and constructing unfinished foundations
		// Check if target building is owned by player or an ally
		if (IsOwnedByAllyOfPlayer(player, cmd.target))
		{
			var entities = FilterEntityList(cmd.entities, player, controlAllUnits);
			var cmpUnitAI = GetFormationUnitAI(entities);
			if (cmpUnitAI)
				cmpUnitAI.Repair(cmd.target, cmd.autocontinue, cmd.queued);
		}
		break;

	case "gather":
		var entities = FilterEntityList(cmd.entities, player, controlAllUnits);
		var cmpUnitAI = GetFormationUnitAI(entities);
		if (cmpUnitAI)
			cmpUnitAI.Gather(cmd.target, cmd.queued);
		break;

	case "returnresource":
		// Check dropsite is owned by player
		if (IsOwnedByPlayer(cmd.target, player))
		{
			var entities = FilterEntityList(cmd.entities, player, controlAllUnits);
			var cmpUnitAI = GetFormationUnitAI(entities);
			if (cmpUnitAI)
				cmpUnitAI.ReturnResource(cmd.target, cmd.queued);
		}
		break;

	case "train":
		if (CanControlUnit(cmd.entity, player, controlAllUnits))
		{
			var queue = Engine.QueryInterface(cmd.entity, IID_TrainingQueue);
			if (queue)
				queue.AddBatch(cmd.template, +cmd.count, cmd.metadata);
		}
		break;

	case "stop-train":
		if (CanControlUnit(cmd.entity, player, controlAllUnits))
		{
			var queue = Engine.QueryInterface(cmd.entity, IID_TrainingQueue);
			if (queue)
				queue.RemoveBatch(cmd.id);
		}
		break;

	case "construct":
		// Message structure:
		// {
		//   "type": "construct",
		//   "entities": [...],
		//   "template": "...",
		//   "x": ...,
		//   "z": ...,
		//   "angle": ...,
		//   "autorepair": true, // whether to automatically start constructing/repairing the new foundation
		//   "autocontinue": true, // whether to automatically gather/build/etc after finishing this
		//   "queued": true,
		// }

		/*
		 * Construction process:
		 *  . Take resources away immediately.
		 *  . Create a foundation entity with 1hp, 0% build progress.
		 *  . Increase hp and build progress up to 100% when people work on it.
		 *  . If it's destroyed, an appropriate fraction of the resource cost is refunded.
		 *  . If it's completed, it gets replaced with the real building.
		 */
		 
		// Check that we can control these units
		var entities = FilterEntityList(cmd.entities, player, controlAllUnits);
		if (!entities.length)
			break;

		// Tentatively create the foundation (we might find later that it's a invalid build command)
		var ent = Engine.AddEntity("foundation|" + cmd.template);
		// TODO: report errors (e.g. invalid template names)

		// Move the foundation to the right place
		var cmpPosition = Engine.QueryInterface(ent, IID_Position);
		cmpPosition.JumpTo(cmd.x, cmd.z);
		cmpPosition.SetYRotation(cmd.angle);

		// Check whether it's obstructed by other entities
		var cmpObstruction = Engine.QueryInterface(ent, IID_Obstruction);
		if (cmpObstruction && cmpObstruction.CheckFoundationCollisions())
		{
			// TODO: report error to player (the building site was obstructed)
			print("Building site was obstructed\n");

			// Remove the foundation because the construction was aborted
			Engine.DestroyEntity(ent);

			break;
		}

		/* TODO: the AI isn't smart enough to explore before building, so we'll
		 * just disable the requirement that the location is visible. Should we
		 * fix that, or let players build in fog too, or something?

		// Check whether it's in a visible region
		var cmpRangeManager = Engine.QueryInterface(SYSTEM_ENTITY, IID_RangeManager);
		var visible = (cmpRangeManager.GetLosVisibility(ent, player) == "visible");
		if (!visible)
		{
			// TODO: report error to player (the building site was not visible)
			print("Building site was not visible\n");

			Engine.DestroyEntity(ent);
			break;
		}
		*/

		var cmpBuildRestrictions = Engine.QueryInterface(ent, IID_BuildRestrictions);
		var cmpBuildLimits = Engine.QueryInterface(playerEnt, IID_BuildLimits);
		if (!cmpBuildLimits.AllowedToBuild(cmpBuildRestrictions.GetCategory()))
		{
			Engine.DestroyEntity(ent);
			break;
		}
		
		var cmpCost = Engine.QueryInterface(ent, IID_Cost);
		if (!cmpPlayer.TrySubtractResources(cmpCost.GetResourceCosts()))
		{
			// TODO: report error to player (they ran out of resources)
			Engine.DestroyEntity(ent);
			break;
		}
		
		// Make it owned by the current player
		var cmpOwnership = Engine.QueryInterface(ent, IID_Ownership);
		cmpOwnership.SetOwner(player);

		// Initialise the foundation
		var cmpFoundation = Engine.QueryInterface(ent, IID_Foundation);
		cmpFoundation.InitialiseConstruction(player, cmd.template);

		// Tell the units to start building this new entity
		if (cmd.autorepair)
		{
			ProcessCommand(player, {
				"type": "repair",
				"entities": entities,
				"target": ent,
				"autocontinue": cmd.autocontinue,
				"queued": cmd.queued
			});
		}

		break;
	
	case "delete-entities":
		var entities = FilterEntityList(cmd.entities, player, controlAllUnits);
		for each (var ent in entities)
		{
			var cmpHealth = Engine.QueryInterface(ent, IID_Health);
			if (cmpHealth)
				cmpHealth.Kill();
			else
				Engine.DestroyEntity(ent);
		}
		break;

	case "set-rallypoint":
		var entities = FilterEntityList(cmd.entities, player, controlAllUnits);
		for each (var ent in entities)
		{
			var cmpRallyPoint = Engine.QueryInterface(ent, IID_RallyPoint);
			if (cmpRallyPoint)
				cmpRallyPoint.SetPosition(cmd.x, cmd.z);
		}
		break;

	case "unset-rallypoint":
		var entities = FilterEntityList(cmd.entities, player, controlAllUnits);
		for each (var ent in entities)
		{
			var cmpRallyPoint = Engine.QueryInterface(ent, IID_RallyPoint);
			if (cmpRallyPoint)
				cmpRallyPoint.Unset();
		}
		break;
		
	case "defeat-player":
		// Send "OnPlayerDefeated" message to player
		Engine.PostMessage(playerEnt, MT_PlayerDefeated, null);
		break;

	case "garrison":
		if (CanControlUnit(cmd.target, player, controlAllUnits))
		{
			var entities = FilterEntityList(cmd.entities, player, controlAllUnits);
			var cmpUnitAI = GetFormationUnitAI(entities);
			if (cmpUnitAI)
				cmpUnitAI.Garrison(cmd.target);
		}
		break;
		
	case "unload":
		if (CanControlUnit(cmd.garrisonHolder, player, controlAllUnits))
		{
			var cmpGarrisonHolder = Engine.QueryInterface(cmd.garrisonHolder, IID_GarrisonHolder);
			if (cmpGarrisonHolder)
				cmpGarrisonHolder.Unload(cmd.entity);
		}
		break;
		
	case "unload-all":
		if (CanControlUnit(cmd.garrisonHolder, player, controlAllUnits))
		{
			var cmpGarrisonHolder = Engine.QueryInterface(cmd.garrisonHolder, IID_GarrisonHolder);
			cmpGarrisonHolder.UnloadAll();
		}
		break;

	case "formation":
		var entities = FilterEntityList(cmd.entities, player, controlAllUnits);
		var cmpUnitAI = GetFormationUnitAI(entities);
		if (!cmpUnitAI)
			break;
		var cmpFormation = Engine.QueryInterface(cmpUnitAI.entity, IID_Formation);
		if (!cmpFormation)
			break;
		cmpFormation.LoadFormation(cmd.name);
		cmpFormation.MoveMembersIntoFormation(true);
		break;

	case "promote":
		var cmpGuiInterface = Engine.QueryInterface(SYSTEM_ENTITY, IID_GuiInterface);
		cmpGuiInterface.PushNotification({"type": "chat", "player": player, "message": "(Cheat - promoted units)"});

		for each (var ent in cmd.entities)
		{
			var cmpPromotion = Engine.QueryInterface(ent, IID_Promotion);
			if (cmpPromotion)
				cmpPromotion.IncreaseXp(cmpPromotion.GetRequiredXp() - cmpPromotion.GetCurrentXp());
		}
		break;

	case "stance":
		var entities = FilterEntityList(cmd.entities, player, controlAllUnits);
		for each (var ent in entities)
		{
			var cmpUnitAI = Engine.QueryInterface(ent, IID_UnitAI);
			if (cmpUnitAI)
				cmpUnitAI.SwitchToStance(cmd.name);
		}
		break;

	default:
		error("Ignoring unrecognised command type '" + cmd.type + "'");
	}
}

/**
 * Get some information about the formations used by entities.
 */
function ExtractFormations(ents)
{
	var entities = []; // subset of ents that have UnitAI
	var members = {}; // { formationentity: [ent, ent, ...], ... }
	for each (var ent in ents)
	{
		var cmpUnitAI = Engine.QueryInterface(ent, IID_UnitAI);
		if (cmpUnitAI)
		{
			var fid = cmpUnitAI.GetFormationController();
			if (fid != INVALID_ENTITY)
			{
				if (!members[fid])
					members[fid] = [];
				members[fid].push(ent);
			}
			entities.push(ent);
		}
	}

	var ids = [ id for (id in members) ];

	return { "entities": entities, "members": members, "ids": ids };
}

/**
 * Remove the given list of entities from their current formations.
 */
function RemoveFromFormation(ents)
{
	var formation = ExtractFormations(ents);
	for (var fid in formation.members)
	{
		var cmpFormation = Engine.QueryInterface(+fid, IID_Formation);
		if (cmpFormation)
			cmpFormation.RemoveMembers(formation.members[fid]);
	}
}

/**
 * Return null or a UnitAI belonging either to the selected unit
 * or to a formation entity for the selected group of units.
 */
function GetFormationUnitAI(ents)
{
	// If an individual was selected, remove it from any formation
	// and command it individually
	if (ents.length == 1)
	{
		RemoveFromFormation(ents);

		return Engine.QueryInterface(ents[0], IID_UnitAI);
	}
	
	// Find what formations the selected entities are currently in
	var formation = ExtractFormations(ents);

	if (formation.entities.length == 0)
	{
		// No units with AI - nothing to do here
		return null;
	}

	var formationEnt = undefined;
	if (formation.ids.length == 1)
	{
		// Selected units all belong to the same formation.
		// Check that it doesn't have any other members
		var fid = formation.ids[0];
		var cmpFormation = Engine.QueryInterface(+fid, IID_Formation);
		if (cmpFormation && cmpFormation.GetMemberCount() == formation.entities.length)
		{
			// The whole formation was selected, so reuse its controller for this command
			formationEnt = +fid;
		}
	}

	if (!formationEnt)
	{
		// We need to give the selected units a new formation controller

		// Remove selected units from their current formation
		for (var fid in formation.members)
		{
			var cmpFormation = Engine.QueryInterface(+fid, IID_Formation);
			if (cmpFormation)
				cmpFormation.RemoveMembers(formation.members[fid]);
		}

		// Create the new controller
		formationEnt = Engine.AddEntity("special/formation");
		var cmpFormation = Engine.QueryInterface(formationEnt, IID_Formation);
		cmpFormation.SetMembers(formation.entities);

		// If all the selected units were previously in formations of the same shape,
		// then set this new formation to that shape too; otherwise use the default shape
		var lastFormationName = undefined;
		for each (var ent in formation.entities)
		{
			var cmpUnitAI = Engine.QueryInterface(ent, IID_UnitAI);
			if (cmpUnitAI)
			{
				var name = cmpUnitAI.GetLastFormationName();
				if (lastFormationName === undefined)
				{
					lastFormationName = name;
				}
				else if (lastFormationName != name)
				{
					lastFormationName = undefined;
					break;
				}
			}
		}
		var formationName;
		if (lastFormationName)
			formationName = lastFormationName;
		else
			formationName = "Line Closed";

		if (CanMoveEntsIntoFormation(formation.entities, formationName))
		{
			cmpFormation.LoadFormation(formationName);
		}
		else
		{
			cmpFormation.LoadFormation("Loose");
		}
	}

	return Engine.QueryInterface(formationEnt, IID_UnitAI);
}

function CanMoveEntsIntoFormation(ents, formationName)
{
	var count = ents.length;
	var classesRequired;

	// TODO: should check the player's civ is allowed to use this formation

	if (formationName == "Loose")
	{
		return true;
	}
	else if (formationName == "Box")
	{
		if (count < 4)
			return false;
	}
	else if (formationName == "Column Closed")
	{
	}
	else if (formationName == "Line Closed")
	{
	}
	else if (formationName == "Column Open")
	{
	}
	else if (formationName == "Line Open")
	{
	}
	else if (formationName == "Flank")
	{
		if (count < 8)
			return false;
	}
	else if (formationName == "Skirmish")
	{
		classesRequired = ["Ranged"];
	}
	else if (formationName == "Wedge")
	{
		if (count < 3)
			return false;
		classesRequired = ["Cavalry"];
	}
	else if (formationName == "Formation12")
	{
	}
	else if (formationName == "Phalanx")
	{
		if (count < 10)
			return false;
		classesRequired = ["Melee", "Infantry"];
	}
	else if (formationName == "Syntagma")
	{
		if (count < 9)
			return false;
		classesRequired = ["Melee", "Infantry"]; // TODO: pike only
	}
	else if (formationName == "Testudo")
	{
		if (count < 9)
			return false;
		classesRequired = ["Melee", "Infantry"];
	}
	else
	{
		return false;
	}

	var looseOnlyUnits = true;
	for each (var ent in ents)
	{
		var cmpIdentity = Engine.QueryInterface(ent, IID_Identity);
		if (cmpIdentity)
		{
			var classes = cmpIdentity.GetClassesList();
			if (looseOnlyUnits && (classes.indexOf("Worker") == -1 || classes.indexOf("Support") == -1))
				looseOnlyUnits = false;
			for each (var classRequired in classesRequired)
			{
				if (classes.indexOf(classRequired) == -1)
				{
					return false;
				}
			}
		}
	}

	if (looseOnlyUnits)
		return false;

	return true;
}

/**
 * Check if entity is owned by player
 */
function IsOwnedByPlayer(entity, player)
{
	var cmpOwnership = Engine.QueryInterface(entity, IID_Ownership);
	return (cmpOwnership && cmpOwnership.GetOwner() == player);
}

/**
 * Check if player can control this entity
 * returns: true if the entity is valid and owned by the player if
 *		or control all units is activated for the player, else false
 */
function CanControlUnit(entity, player, controlAll)
{
	return (IsOwnedByPlayer(entity, player) || controlAll);
}

/**
 * Filter entities which the player can control
 */
function FilterEntityList(entities, player, controlAll)
{
	return entities.filter(function(ent) { return CanControlUnit(ent, player, controlAll);} );
}

Engine.RegisterGlobal("CanMoveEntsIntoFormation", CanMoveEntsIntoFormation);
Engine.RegisterGlobal("ProcessCommand", ProcessCommand);
