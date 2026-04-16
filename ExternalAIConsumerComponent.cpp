// Copyright Unchained.

#include "AI/ExternalAIConsumerComponent.h"

#include "AI/ExternalAISubsystem.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Actor.h"

UExternalAIConsumerComponent::UExternalAIConsumerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UExternalAIConsumerComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetWorld() && GetWorld()->GetGameInstance())
	{
		if (UExternalAISubsystem* ExternalAISubsystem = GetWorld()->GetGameInstance()->GetSubsystem<UExternalAISubsystem>())
		{
			ExternalAISubsystem->OnResponseReceived.AddDynamic(this, &UExternalAIConsumerComponent::HandleSubsystemResponse);
		}
	}
}

void UExternalAIConsumerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld() && GetWorld()->GetGameInstance())
	{
		if (UExternalAISubsystem* ExternalAISubsystem = GetWorld()->GetGameInstance()->GetSubsystem<UExternalAISubsystem>())
		{
			ExternalAISubsystem->OnResponseReceived.RemoveDynamic(this, &UExternalAIConsumerComponent::HandleSubsystemResponse);
		}
	}

	Super::EndPlay(EndPlayReason);
}

FString UExternalAIConsumerComponent::RequestAIUpdate(const int32 PlayerReputation, const FString& Location, const bool bActiveAlert, const FString& AdditionalContextJson)
{
	if (!GetWorld() || !GetWorld()->GetGameInstance())
	{
		LastErrorMessage = TEXT("GameInstance was not available for the external AI request.");
		OnAIStateUpdated.Broadcast(FExternalAIResponse(), false, LastErrorMessage);
		return FString();
	}

	UExternalAISubsystem* ExternalAISubsystem = GetWorld()->GetGameInstance()->GetSubsystem<UExternalAISubsystem>();
	if (!ExternalAISubsystem)
	{
		LastErrorMessage = TEXT("ExternalAISubsystem was not available.");
		OnAIStateUpdated.Broadcast(FExternalAIResponse(), false, LastErrorMessage);
		return FString();
	}

	FExternalAIRequest RequestData;
	RequestData.NpcId = ResolveNpcId();
	RequestData.PlayerReputation = PlayerReputation;
	RequestData.Location = ResolveLocationString(Location);
	RequestData.bActiveAlert = bActiveAlert;
	RequestData.AdditionalContextJson = AdditionalContextJson;
	RequestData.DefaultMood = DefaultMood;

	LastRequestId = ExternalAISubsystem->RequestNPCDecision(RequestData);
	LastErrorMessage.Reset();

	return LastRequestId;
}

void UExternalAIConsumerComponent::ApplyAIResponse(const FExternalAIResponse& Response)
{
	CurrentMood = Response.Mood;
	CurrentThreatScore = Response.ThreatScore;
	LastRecommendedAction = Response.RecommendedAction;
	LastReplyText = Response.ReplyText;
}

void UExternalAIConsumerComponent::HandleSubsystemResponse(const FString& RequestId, const FExternalAIResponse& Response, const bool bSuccess, const FString& ErrorMessage)
{
	if (RequestId != LastRequestId)
	{
		return;
	}

	LastErrorMessage = ErrorMessage;

	if (bSuccess)
	{
		ApplyAIResponse(Response);
	}

	OnAIStateUpdated.Broadcast(Response, bSuccess, ErrorMessage);
}

FString UExternalAIConsumerComponent::ResolveNpcId() const
{
	if (!NpcIdOverride.IsEmpty())
	{
		return NpcIdOverride;
	}

	return GetOwner() ? GetOwner()->GetName() : TEXT("UnknownNPC");
}

FString UExternalAIConsumerComponent::ResolveLocationString(const FString& ExplicitLocation) const
{
	if (!ExplicitLocation.IsEmpty())
	{
		return ExplicitLocation;
	}

	if (!DefaultLocation.IsEmpty())
	{
		return DefaultLocation;
	}

	if (const AActor* Owner = GetOwner())
	{
		const FVector ActorLocation = Owner->GetActorLocation();
		return FString::Printf(TEXT("X=%.0f Y=%.0f Z=%.0f"), ActorLocation.X, ActorLocation.Y, ActorLocation.Z);
	}

	return TEXT("UnknownLocation");
}
