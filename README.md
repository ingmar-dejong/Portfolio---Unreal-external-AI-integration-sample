# Unreal External AI Dialogue Sample

Small Unreal Engine sample that shows how an NPC can request an external AI state and expose the result inside gameplay and dialogue.

At the current stage, the sample can:
- send an AI update request for an NPC
- receive a response from a local backend
- store values such as mood, threat score, recommended action, and reply text
- display that data in gameplay or Narrative dialogue

## Current Result

The current in-editor test proves that an NPC can expose its current mood through dialogue.

Example values returned by the sample backend:
- `calm`
- `suspicious`
- `aggressive`

The same response also includes:
- `reply_text`
- `recommended_action`
- `threat_score`

## Demo Video

Demo video:
- https://onedrive.live.com/?id=%2Fpersonal%2F9d9f3cd318aeacbc%2FDocuments%2FPortfolio%2FUnreal%2Dexternal%2DAI%2Dintegration%2Dsample&view=0

In the video, the player asks the NPC what her mood is. The NPC answers using the current value returned by the sample backend, which is currently hard coded to values such as `calm`.

## System Overview

The system is split into three parts:

1. Unreal request layer
   - `ExternalAISubsystem.h/.cpp`
   - Sends HTTP requests and parses the backend response.

2. NPC-facing gameplay layer
   - `ExternalAIConsumerComponent.h/.cpp`
   - Lives on an NPC actor.
   - Requests updates through the subsystem.
   - Stores the latest AI state in Blueprint-readable properties such as `CurrentMood` and `LastReplyText`.

3. Local backend
   - `server.js`
   - Returns mock AI behavior based on simple inputs like reputation, alert state, location, and extra context.

## Dialogue Integration

The dialogue side works by reading values that were already fetched and stored on the NPC component.

Typical flow:

1. An interaction or event calls `RequestAIUpdate(...)` on the NPC's `ExternalAIConsumerComponent`.
2. The component sends the request through `UExternalAISubsystem`.
3. The backend returns a response.
4. The component stores the result in:
   - `CurrentMood`
   - `CurrentThreatScore`
   - `LastRecommendedAction`
   - `LastReplyText`
5. Dialogue reads those values and injects them into text placeholders.

This means dialogue is currently reading AI state, not generating it on the spot.

## Backend Behavior

The included Node backend is intentionally simple. It reacts to:
- player reputation
- active alert state
- location
- optional context such as `quest_state`

Based on that input it returns a lightweight NPC decision, for example:
- calm + ignore
- suspicious + warn player
- aggressive + raise alarm

## Files

- `ExternalAISubsystem.h/.cpp`: HTTP request/response layer
- `ExternalAIConsumerComponent.h/.cpp`: NPC component that stores the latest AI state
- `ExternalAIIntegrationTypes.h`: request and response structs
- `ExternalAISettings.h`: settings for the integration
- `server.js`: local sample backend
- `package.json`: backend start script

## Running The Backend

By default the backend runs on `http://127.0.0.1:8080`.

Available routes:
- `GET /health`
- `POST /api/npc/decision`

## Design Choises
Why a subsystem? Central layer, no scattered HTTP code in every NPC. Reusable, easy to replace.
Why async? HTTP calls take tens of milliseconds. The game thread must never block, that would/could cause visible hitches.
Why RequestId filtering? Multiple NPCs can send requests at the same time. Without filtering, every NPC could react to every response.
Why a backend between Unreal and the model? API keys shouldn’t be in a client build. The backend validates and normalizes AI output before Unreal receives it.
Why “AI advises, Unreal decides”? AI output can sometimes be unpredictable or invalid. Unreal maps it to a whitelist of known gameplay actions to keep things stable.


