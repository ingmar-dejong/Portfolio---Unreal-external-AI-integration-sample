'use strict';

require('dotenv').config();

const http = require('http');
const { Anthropic } = require('@anthropic-ai/sdk');

const PORT = Number(process.env.PORT || 8080);

// Startup check — logs whether the API key was found, without exposing the full key.
if (!process.env.ANTHROPIC_API_KEY) {
  console.error('[external-ai] ERROR: ANTHROPIC_API_KEY not found. Check your .env file.');
} else {
  console.log('[external-ai] API key loaded:', process.env.ANTHROPIC_API_KEY.slice(0, 14) + '...');
}

const anthropic = new Anthropic({ apiKey: process.env.ANTHROPIC_API_KEY });

// Whitelists used to validate the AI response before sending it to Unreal.
// The AI may return unexpected values — we never pass those through.
const VALID_MOODS = new Set(['calm', 'suspicious', 'aggressive']);
const VALID_ACTIONS = new Set(['Ignore', 'WarnPlayer', 'RaiseAlarm']);

function sendJson(response, statusCode, payload) {
  response.writeHead(statusCode, {
    'Content-Type': 'application/json; charset=utf-8',
    'Access-Control-Allow-Origin': '*',
    'Access-Control-Allow-Methods': 'POST, OPTIONS',
    'Access-Control-Allow-Headers': 'Content-Type'
  });

  response.end(JSON.stringify(payload));
}

function normalizeString(value, fallback = '') {
  return typeof value === 'string' ? value : fallback;
}

function normalizeNumber(value, fallback = 0) {
  return Number.isFinite(value) ? value : fallback;
}

function normalizeBoolean(value, fallback = false) {
  return typeof value === 'boolean' ? value : fallback;
}

// Builds the two messages sent to Claude:
// - system: who the NPC is and what format to respond in
// - user: what the current in-game situation is
function buildMessages(npcId, playerReputation, location, activeAlert, defaultMood, context) {
  const npcDescription = normalizeString(context.npc_description);
  const questState = normalizeString(context.quest_state, 'none');
  const timeOfDay = context.time_of_day !== undefined ? String(context.time_of_day) : 'unknown';

  const systemLines = [
    `You are ${npcId}, an NPC in a medieval game.`,
    npcDescription ? npcDescription : 'You are a resident of this town.',
  ];

  if (defaultMood) {
    systemLines.push(`Your base mood and demeanor is: ${defaultMood}. Let this influence how you speak and react.`);
  }

  systemLines.push(
    '',
    'Evaluate the current situation and decide how to respond.',
    'Respond only with a single valid JSON object. Do not include any explanation or extra text.',
    '',
    'Required fields:',
    '  "mood"               — exactly one of: "calm", "suspicious", "aggressive"',
    '  "threat_score"       — a number between 0.0 (no threat) and 1.0 (maximum threat)',
    '  "recommended_action" — exactly one of: "Ignore", "WarnPlayer", "RaiseAlarm"',
    '  "reply_text"         — one sentence the NPC would say out loud in this situation'
  );

  const system = systemLines.join('\n');

  const user = [
    'Current situation:',
    `  Player reputation : ${playerReputation}`,
    `  Location          : ${location}`,
    `  Active alert      : ${activeAlert}`,
    `  Quest state       : ${questState}`,
    `  Time of day       : ${timeOfDay}`,
    '',
    'How do you respond?'
  ].join('\n');

  return { system, user };
}

// Calls the Claude API and returns the raw response text.
async function callClaude(system, user) {
  const response = await anthropic.messages.create({
    model: 'claude-haiku-4-5-20251001', // fast and cheap, good for gameplay decisions
    max_tokens: 150,
    system,
    messages: [{ role: 'user', content: user }]
  });

  return response.content[0].text;
}

// Parses the raw text returned by Claude into a validated decision object.
// Falls back to safe defaults if any field is missing or invalid.
function parseAndValidate(rawText) {
  let parsed;

  try {
    // Claude sometimes wraps JSON in markdown code fences — strip them first.
    const cleaned = rawText.replace(/^```(?:json)?\s*/i, '').replace(/\s*```$/, '').trim();
    parsed = JSON.parse(cleaned);
  } catch {
    console.warn('[external-ai] Claude returned non-JSON text:', rawText);
    parsed = {};
  }

  return {
    mood: VALID_MOODS.has(parsed.mood) ? parsed.mood : 'calm',
    threat_score: Math.min(1, Math.max(0, Number(parsed.threat_score) || 0)),
    recommended_action: VALID_ACTIONS.has(parsed.recommended_action) ? parsed.recommended_action : 'Ignore',
    reply_text: typeof parsed.reply_text === 'string' ? parsed.reply_text.slice(0, 200) : ''
  };
}

// Orchestrates the full decision pipeline:
// 1. extract and normalize inputs
// 2. build prompt messages
// 3. call Claude
// 4. parse and validate the response
async function buildNpcDecision(payload) {
  const npcId = normalizeString(payload.npc_id, 'UnknownNPC');
  const playerReputation = normalizeNumber(payload.player_reputation, 0);
  const location = normalizeString(payload.location, 'UnknownLocation');
  const activeAlert = normalizeBoolean(payload.active_alert, false);
  const defaultMood = normalizeString(payload.default_mood);
  const context = payload.context && typeof payload.context === 'object' ? payload.context : {};

  const { system, user } = buildMessages(npcId, playerReputation, location, activeAlert, defaultMood, context);
  const rawText = await callClaude(system, user);

  console.log('[external-ai] Claude raw response:', rawText);

  return parseAndValidate(rawText);
}

async function handleNpcDecision(request, response) {
  let rawBody = '';

  request.on('data', (chunk) => {
    rawBody += chunk;

    if (rawBody.length > 1024 * 1024) {
      request.destroy();
    }
  });

  request.on('end', async () => {
    let payload;

    try {
      payload = rawBody ? JSON.parse(rawBody) : {};
    } catch {
      sendJson(response, 400, { error: 'Invalid JSON payload.' });
      return;
    }

    console.log('[external-ai] request:', payload);

    try {
      const decision = await buildNpcDecision(payload);
      console.log('[external-ai] response:', decision);
      sendJson(response, 200, decision);
    } catch (err) {
      console.error('[external-ai] Claude API error:', err.message);
      sendJson(response, 502, { error: 'AI backend unavailable.' });
    }
  });
}

const server = http.createServer((request, response) => {
  if (request.method === 'OPTIONS') {
    sendJson(response, 204, {});
    return;
  }

  if (request.method === 'GET' && request.url === '/health') {
    sendJson(response, 200, { ok: true, service: 'external-ai-sample-backend' });
    return;
  }

  if (request.method === 'POST' && request.url === '/api/npc/decision') {
    handleNpcDecision(request, response);
    return;
  }

  sendJson(response, 404, { error: 'Route not found.' });
});

server.listen(PORT, () => {
  console.log(`[external-ai] backend listening on http://127.0.0.1:${PORT}`);
});
