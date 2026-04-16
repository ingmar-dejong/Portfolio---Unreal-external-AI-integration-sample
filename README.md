# Unreal External AI Integration Sample

NPC requests a decision from Claude, gets back mood and reply text, says it in dialogue widget.

## Demo Video

https://1drv.ms/f/c/9d9f3cd318aeacbc/IgB9Ayrng5_6TpWXUGxpQBMrAach5-tBwzFf9fG-75oS9Oo?e=qqlv7x

The player asks the NPC what her mood is. The NPC answers using a live response from Claude, generated based on her identity, location, base mood, and the current game context.

## How it works

1. NPC sends game context to a local Node.js backend
2. Backend calls Claude, gets back mood/threat/action/reply
3. NPC stores the result on `ExternalAIConsumerComponent`
4. Dialogue reads `LastReplyText` via `{AIReply}`

## Parts

**`ExternalAISubsystem`** — one instance per game session, handles HTTP + mock mode, broadcasts results via delegate

**`ExternalAIConsumerComponent`** — on the NPC, calls `RequestAIUpdate()`, filters by RequestId so multiple NPCs don't mix up responses, fires `OnAIStateUpdated` when done

**`server.js`** — Node backend on port 8080, builds prompt from game context, calls Claude, validates response before returning

## Design Choices

**Why a subsystem?** Central layer, no scattered HTTP code in every NPC. Reusable, easy to replace.

**Why async?** HTTP calls take tens of milliseconds. The game thread must never block, that would cause visible hitches.

**Why RequestId filtering?** Multiple NPCs can send requests at the same time. Without filtering, every NPC could react to every response.

**Why a backend between Unreal and the model?** API keys shouldn't be in a client build. The backend validates and normalizes AI output before Unreal receives it.

**Why "AI advises, Unreal decides"?** AI output can sometimes be unpredictable or invalid. Unreal maps it to a whitelist of known gameplay actions to keep things stable.

## NPC setup

```
NpcIdOverride         = Jane
DefaultLocation       = Old Farm
DefaultMood           = Tired
AdditionalContextJson = {"npc_description": "You are Jane. A farmer who lived at this farm her whole life. You are calm but firm."}
```

Claude gets: who the NPC is, her description, her base mood, and the current situation.

## Dialogue integration

In the Narrative dialogue asset, override `GetStringVariable`:

```
GetNPCExternalAIConsumerFromDialogue(self, Node)
→ Is Valid?
    True  → VariableName == "AIReply"? → Return LastReplyText
    False → Return Parent GetStringVariable
```

`GetNPCExternalAIConsumerFromDialogue` finds the NPC by iterating `Dialogue->Speakers` and checking `SpeakerAvatar` for an `ExternalAIConsumerComponent`. Bypasses SpeakerID lookup which is unreliable during variable resolution.

Put `{AIReply}` in the NPC dialogue node text.

## Project Settings

**Edit → Project Settings → Game → External AI**
- `Use Mock Responses` — false to use Claude
- `Endpoint Url` — `http://127.0.0.1:8080/api/npc/decision`

## Backend setup

```

copy .env.example .env   # fill in ANTHROPIC_API_KEY
node start
```

## Test (PowerShell)

```powershell
$uri = "http://127.0.0.1:8080/api/npc/decision"
$body = '{"npc_id": "Jane", "player_reputation": 0, "location": "Old Farm", "active_alert": false, "default_mood": "Tired", "context": {"npc_description": "You are Jane. A farmer who lived at this farm her whole life. You are calm but firm."}}'
Invoke-RestMethod -Method Post -Uri $uri -ContentType "application/json" -Body $body
```

## Files

| File | What |
|---|---|
| `ExternalAISubsystem.h/.cpp` | HTTP layer |
| `ExternalAIConsumerComponent.h/.cpp` | NPC component |
| `ExternalAIIntegrationTypes.h` | Request/response structs |
| `ExternalAISettings.h` | Project Settings config |
| `server.js` | Claude backend |
| `.env.example` | API key template |
