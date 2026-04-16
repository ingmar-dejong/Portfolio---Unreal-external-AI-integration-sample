// Copyright Unchained.

#pragma once

#include "CoreMinimal.h"
#include "ExternalAIIntegrationTypes.generated.h"

USTRUCT(BlueprintType)
struct UNCHAINED_API FExternalAIRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "External AI")
	FString NpcId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "External AI")
	int32 PlayerReputation = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "External AI")
	FString Location;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "External AI")
	bool bActiveAlert = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "External AI")
	FString AdditionalContextJson;

	// Base mood/demeanor set by the developer. Guides Claude's response without overriding it.
	// Examples: "Tired", "Suspicious", "Cheerful", "Grieving"
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "External AI")
	FString DefaultMood;
};

USTRUCT(BlueprintType)
struct UNCHAINED_API FExternalAIResponse
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "External AI")
	FString Mood;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "External AI")
	float ThreatScore = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "External AI")
	FString RecommendedAction;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "External AI")
	FString ReplyText;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "External AI")
	FString RawResponseJson;
};
