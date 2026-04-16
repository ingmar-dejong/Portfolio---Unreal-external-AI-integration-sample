// Copyright Unchained.

#include "AI/ExternalAISubsystem.h"

#include "AI/ExternalAISettings.h"
#include "Dom/JsonObject.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "TimerManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	static const FString ExternalAIMoodCalm = TEXT("calm");
	static const FString ExternalAIMoodSuspicious = TEXT("suspicious");
	static const FString ExternalAIMoodAggressive = TEXT("aggressive");
	static const FString ExternalAIActionIgnore = TEXT("Ignore");
	static const FString ExternalAIActionWarnPlayer = TEXT("WarnPlayer");
	static const FString ExternalAIActionRaiseAlarm = TEXT("RaiseAlarm");
}

FString UExternalAISubsystem::RequestNPCDecision(const FExternalAIRequest& RequestData)
{
	const FString RequestId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
	const UExternalAISettings* Settings = GetDefault<UExternalAISettings>();

	if (!Settings || Settings->bUseMockResponses || Settings->EndpointUrl.IsEmpty())
	{
		CompleteMockRequest(RequestId, RequestData);
		return RequestId;
	}

	const TSharedPtr<FJsonObject> JsonObject = BuildRequestJson(RequestData);
	if (!JsonObject.IsValid())
	{
		BroadcastResult(RequestId, FExternalAIResponse(), false, TEXT("Failed to build external AI request payload."));
		return RequestId;
	}

	FString RequestBody;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
	if (!FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer))
	{
		BroadcastResult(RequestId, FExternalAIResponse(), false, TEXT("Failed to serialize external AI request payload."));
		return RequestId;
	}

	if (Settings->bLogPayloads)
	{
		UE_LOG(LogTemp, Log, TEXT("External AI request %s payload: %s"), *RequestId, *RequestBody);
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(Settings->EndpointUrl);
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetHeader(TEXT("Accept"), TEXT("application/json"));
	HttpRequest->SetTimeout(Settings->RequestTimeoutSeconds);
	HttpRequest->SetContentAsString(RequestBody);
	HttpRequest->OnProcessRequestComplete().BindUObject(this, &UExternalAISubsystem::HandleHttpResponse, RequestId);
	HttpRequest->ProcessRequest();

	return RequestId;
}

bool UExternalAISubsystem::IsUsingMockResponses() const
{
	if (const UExternalAISettings* Settings = GetDefault<UExternalAISettings>())
	{
		return Settings->bUseMockResponses || Settings->EndpointUrl.IsEmpty();
	}

	return true;
}

void UExternalAISubsystem::CompleteMockRequest(const FString& RequestId, const FExternalAIRequest RequestData)
{
	if (UWorld* World = GetWorld())
	{
		FTimerDelegate MockCallback;
		MockCallback.BindLambda([this, RequestId, RequestData]()
		{
			FExternalAIResponse Response = BuildMockResponse(RequestData);

			if (const UExternalAISettings* Settings = GetDefault<UExternalAISettings>(); Settings && Settings->bLogPayloads)
			{
				UE_LOG(LogTemp, Log, TEXT("External AI request %s resolved through mock response."), *RequestId);
			}

			BroadcastResult(RequestId, Response, true, FString());
		});

		FTimerHandle TimerHandle;
		World->GetTimerManager().SetTimerForNextTick(MockCallback);
		return;
	}

	FExternalAIResponse Response = BuildMockResponse(RequestData);
	BroadcastResult(RequestId, Response, true, FString());
}

void UExternalAISubsystem::HandleHttpResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, FString RequestId)
{
	if (!bWasSuccessful || !Response.IsValid())
	{
		BroadcastResult(RequestId, FExternalAIResponse(), false, TEXT("External AI request failed before a valid response was returned."));
		return;
	}

	FExternalAIResponse ParsedResponse;
	const FString ResponseString = Response->GetContentAsString();
	if (!TryParseResponseJson(ResponseString, ParsedResponse))
	{
		BroadcastResult(RequestId, FExternalAIResponse(), false, TEXT("External AI response JSON could not be parsed."));
		return;
	}

	ParsedResponse.RawResponseJson = ResponseString;

	if (const UExternalAISettings* Settings = GetDefault<UExternalAISettings>(); Settings && Settings->bLogPayloads)
	{
		UE_LOG(LogTemp, Log, TEXT("External AI response %s payload: %s"), *RequestId, *ResponseString);
	}

	BroadcastResult(RequestId, ParsedResponse, true, FString());
}

void UExternalAISubsystem::BroadcastResult(const FString& RequestId, const FExternalAIResponse& ResponseData, const bool bSuccess, const FString& ErrorMessage)
{
	OnResponseReceived.Broadcast(RequestId, ResponseData, bSuccess, ErrorMessage);
}

TSharedPtr<FJsonObject> UExternalAISubsystem::BuildRequestJson(const FExternalAIRequest& RequestData) const
{
	TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();
	JsonObject->SetStringField(TEXT("npc_id"), RequestData.NpcId);
	JsonObject->SetNumberField(TEXT("player_reputation"), RequestData.PlayerReputation);
	JsonObject->SetStringField(TEXT("location"), RequestData.Location);
	JsonObject->SetBoolField(TEXT("active_alert"), RequestData.bActiveAlert);

	if (!RequestData.DefaultMood.IsEmpty())
	{
		JsonObject->SetStringField(TEXT("default_mood"), RequestData.DefaultMood);
	}

	if (!RequestData.AdditionalContextJson.IsEmpty())
	{
		TSharedPtr<FJsonObject> ContextObject;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RequestData.AdditionalContextJson);
		if (FJsonSerializer::Deserialize(Reader, ContextObject) && ContextObject.IsValid())
		{
			JsonObject->SetObjectField(TEXT("context"), ContextObject);
		}
		else
		{
			JsonObject->SetStringField(TEXT("context_raw"), RequestData.AdditionalContextJson);
		}
	}

	return JsonObject;
}

bool UExternalAISubsystem::TryParseResponseJson(const FString& ResponseString, FExternalAIResponse& OutResponse) const
{
	TSharedPtr<FJsonObject> JsonObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseString);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		return false;
	}

	JsonObject->TryGetStringField(TEXT("mood"), OutResponse.Mood);
	JsonObject->TryGetNumberField(TEXT("threat_score"), OutResponse.ThreatScore);
	JsonObject->TryGetStringField(TEXT("recommended_action"), OutResponse.RecommendedAction);
	JsonObject->TryGetStringField(TEXT("reply_text"), OutResponse.ReplyText);

	return !OutResponse.RecommendedAction.IsEmpty() || !OutResponse.Mood.IsEmpty() || !OutResponse.ReplyText.IsEmpty();
}

FExternalAIResponse UExternalAISubsystem::BuildMockResponse(const FExternalAIRequest& RequestData) const
{
	FExternalAIResponse Response;

	if (RequestData.bActiveAlert || RequestData.PlayerReputation <= -25)
	{
		Response.Mood = ExternalAIMoodAggressive;
		Response.ThreatScore = 0.9f;
		Response.RecommendedAction = ExternalAIActionRaiseAlarm;
		Response.ReplyText = FString::Printf(TEXT("%s identifies the player as a direct threat."), *RequestData.NpcId);
	}
	else if (RequestData.PlayerReputation < 10)
	{
		Response.Mood = ExternalAIMoodSuspicious;
		Response.ThreatScore = 0.65f;
		Response.RecommendedAction = ExternalAIActionWarnPlayer;
		Response.ReplyText = FString::Printf(TEXT("%s issues a warning and keeps distance."), *RequestData.NpcId);
	}
	else
	{
		Response.Mood = ExternalAIMoodCalm;
		Response.ThreatScore = 0.15f;
		Response.RecommendedAction = ExternalAIActionIgnore;
		Response.ReplyText = FString::Printf(TEXT("%s remains calm in %s."), *RequestData.NpcId, *RequestData.Location);
	}

	const TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();
	JsonObject->SetStringField(TEXT("mood"), Response.Mood);
	JsonObject->SetNumberField(TEXT("threat_score"), Response.ThreatScore);
	JsonObject->SetStringField(TEXT("recommended_action"), Response.RecommendedAction);
	JsonObject->SetStringField(TEXT("reply_text"), Response.ReplyText);

	FString RawJson;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RawJson);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
	Response.RawResponseJson = RawJson;

	return Response;
}
