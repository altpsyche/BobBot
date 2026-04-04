// BobBot - Model Context Protocol AI Tool for Unreal Engine

#pragma once

#include "CoreMinimal.h"

class FBobBotPythonBridge;

/**
 * Polls bob_mcp_server.get_status() and tracks whether the MCP server
 * is running and how many clients are connected.
 */
class FBobBotServerMonitor
{
public:
	void Poll(FBobBotPythonBridge& Bridge);

	bool IsRunning() const { return bRunning; }
	int32 GetClientCount() const { return ClientCount; }

private:
	bool bRunning = false;
	int32 ClientCount = 0;
};

/**
 * Polls bob_bridge_launcher.check_health() and handles automatic
 * reconnection when the HTTP bridge goes down unexpectedly.
 * Updates FBobBotRuntimeStatus with bridge health info.
 */
class FBobBotBridgeMonitor
{
public:
	/** Callback type for sending messages back to the chat UI. */
	using FAddMessageFunc = TFunction<void(bool /*bError*/, const FString& /*Message*/)>;

	/** Must be called before first Poll to wire up chat messaging. */
	void SetMessageCallback(FAddMessageFunc InCallback) { MessageCallback = MoveTemp(InCallback); }

	/** Poll interval must be passed in so cooldown math works correctly. */
	void Poll(FBobBotPythonBridge& Bridge, float PollInterval);

	bool IsRunning() const { return bWasRunning; }

private:
	bool bWasRunning = false;
	int32 ReconnectAttempts = 0;
	float ReconnectCooldown = 0.f;
	static constexpr int32 MaxReconnectAttempts = 3;

	FAddMessageFunc MessageCallback;
};

/**
 * One-time poll for claude-agent-sdk availability.
 * Stops polling after the SDK is found or MaxPolls attempts.
 */
class FBobBotSdkMonitor
{
public:
	void Poll(FBobBotPythonBridge& Bridge);

	bool IsDone() const { return bDone; }

private:
	int32 PollCount = 0;
	static constexpr int32 MaxPolls = 60;
	bool bDone = false;
};
