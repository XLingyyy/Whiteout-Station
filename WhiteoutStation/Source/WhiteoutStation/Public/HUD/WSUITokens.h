#pragma once

#include "CoreMinimal.h"

// ============================================================================
// Whiteout Station UI Design Tokens
// 单一可信来源对齐 docs/UI_STYLE_v0.3.md
// 所有程序化 UMG 控件的视觉常量都应从此头文件获取，禁止在 .cpp 中硬编码。
// 颜色沿用项目既有约定：sRGB 归一化值直接作为 FLinearColor 分量（与 v0.4 一致）。
// ============================================================================

namespace WSUITokens
{
	// =========================================================================
	// 颜色 — Surface
	// =========================================================================
	namespace Color
	{
		// 深面板：ESC、证据板主面板
		inline const FLinearColor SurfaceDeep(0.020f, 0.035f, 0.050f, 0.88f);
		// 标准面板：HUD、人物卡、轮盘卡
		inline const FLinearColor SurfacePanel(0.035f, 0.071f, 0.102f, 0.78f);
		// 紧凑表面：局部信息背衬、底部提示
		inline const FLinearColor SurfaceCompact(0.047f, 0.082f, 0.114f, 0.68f);
		// 悬停表面：鼠标悬停、键盘焦点
		inline const FLinearColor SurfaceHover(0.090f, 0.133f, 0.173f, 0.82f);
		// 对话条深底
		inline const FLinearColor SurfaceDialogue(0.020f, 0.035f, 0.050f, 0.94f);
		// 结算/全屏暗底
		inline const FLinearColor SurfaceFullscreen(0.004f, 0.014f, 0.026f, 1.0f);
		// 预览面板底
		inline const FLinearColor SurfacePreview(0.008f, 0.025f, 0.045f, 0.985f);
		// 证据过滤/详情子面板
		inline const FLinearColor SurfaceFilter(0.025f, 0.050f, 0.070f, 0.78f);
		// 对话输入框底
		inline const FLinearColor SurfaceInput(0.025f, 0.055f, 0.075f, 0.98f);
		inline const FLinearColor SurfaceInputFocused(0.045f, 0.095f, 0.125f, 0.98f);

		// 描边
		inline const FLinearColor StrokeHairline(0.863f, 0.906f, 0.933f, 0.12f);
		inline const FLinearColor StrokeHairlineSubtle(0.863f, 0.906f, 0.933f, 0.06f);
		inline const FLinearColor StrokeFocus(0.953f, 0.961f, 0.969f, 0.92f);
		inline const FLinearColor StrokeDivider(0.863f, 0.906f, 0.933f, 0.10f);

		// 文字
		inline const FLinearColor TextPrimary(0.953f, 0.961f, 0.969f, 1.0f);
		inline const FLinearColor TextSecondary(0.722f, 0.760f, 0.792f, 1.0f);
		inline const FLinearColor TextMuted(0.467f, 0.518f, 0.557f, 1.0f);
		inline const FLinearColor TextCinematicWarm(0.82f, 0.92f, 1.0f, 1.0f);

		// 强调色 — 橙色纪律：仅 AP / 警告 / 当前交互目标
		inline const FLinearColor AccentAction(0.949f, 0.549f, 0.157f, 1.0f);
		inline const FLinearColor AccentWarning(0.851f, 0.329f, 0.302f, 1.0f);
		inline const FLinearColor AccentInfo(0.491f, 0.714f, 0.839f, 1.0f);
		inline const FLinearColor AccentSuccess(0.471f, 0.678f, 0.541f, 1.0f);

		// 状态条配色（健康 / 体温 / 精力 / 饥饿 / 压力）
		inline const FLinearColor StatusHealth(0.851f, 0.329f, 0.302f, 1.0f);
		inline const FLinearColor StatusTemperature(0.491f, 0.714f, 0.839f, 1.0f);
		inline const FLinearColor StatusEnergy(0.949f, 0.549f, 0.157f, 1.0f);
		inline const FLinearColor StatusHunger(0.83f, 0.70f, 0.38f, 1.0f);
		inline const FLinearColor StatusPressure(0.72f, 0.50f, 0.78f, 1.0f);

		// 信任条
		inline const FLinearColor TrustBar(0.491f, 0.714f, 0.839f, 1.0f);

		// 进度条背景槽
		inline const FLinearColor ProgressBarBackground(0.025f, 0.050f, 0.070f, 0.65f);

		// 按钮状态
		inline const FLinearColor ButtonNormal(0.055f, 0.10f, 0.14f, 0.55f);
		inline const FLinearColor ButtonHover(0.090f, 0.133f, 0.173f, 0.85f);
		inline const FLinearColor ButtonPressed(0.949f, 0.549f, 0.157f, 0.25f);
		inline const FLinearColor ButtonDisabled(0.055f, 0.10f, 0.14f, 0.35f);

		// 对话意图按钮底
		inline const FLinearColor DialogueChoiceNormal(0.045f, 0.105f, 0.145f, 0.96f);
		inline const FLinearColor DialogueChoiceHover(0.09f, 0.16f, 0.22f, 0.96f);

		// 滑块
		inline const FLinearColor SliderBar(0.09f, 0.13f, 0.17f, 1.0f);
		inline const FLinearColor SliderHandle(0.491f, 0.714f, 0.839f, 1.0f);
	}

	// =========================================================================
	// 模糊强度
	// =========================================================================
	namespace Blur
	{
		constexpr float Compact = 10.0f;
		constexpr float Panel = 16.0f;
		constexpr float Modal = 22.0f;
	}

	// =========================================================================
	// 圆角半径
	// =========================================================================
	namespace Radius
	{
		constexpr float Small = 6.0f;
		constexpr float Panel = 12.0f;
		constexpr float Modal = 16.0f;
	}

	// =========================================================================
	// 间距（基础 4 / 8 / 12 / 16 / 24 / 32）
	// =========================================================================
	namespace Spacing
	{
		constexpr float S4 = 4.0f;
		constexpr float S8 = 8.0f;
		constexpr float S12 = 12.0f;
		constexpr float S16 = 16.0f;
		constexpr float S24 = 24.0f;
		constexpr float S32 = 32.0f;
		constexpr float PaddingSmall = 16.0f;
		constexpr float PaddingLarge = 24.0f;
		constexpr float CardGap = 11.0f;
		constexpr float SafeMargin720 = 20.0f;
		constexpr float SafeMargin1080 = 30.0f;
	}

	// =========================================================================
	// 字号（1080p 基准）
	// =========================================================================
	namespace Type
	{
		constexpr int32 Caption = 14;
		constexpr int32 Body = 16;
		constexpr int32 Label = 18;
		constexpr int32 CardTitle = 20;
		constexpr int32 Section = 24;
		constexpr int32 Screen = 34;
		constexpr int32 Cinematic = 44;

		// 720p 等比缩减
		constexpr int32 Caption720 = 12;
		constexpr int32 Body720 = 14;
		constexpr int32 Label720 = 15;
		constexpr int32 CardTitle720 = 17;
		constexpr int32 Section720 = 20;
		constexpr int32 Screen720 = 28;
		constexpr int32 Cinematic720 = 36;
	}

	// =========================================================================
	// 阴影
	// =========================================================================
	namespace Shadow
	{
		constexpr float OffsetY = 8.0f;
		constexpr float Spread = 28.0f;
		constexpr float Opacity = 0.42f;
	}

	// =========================================================================
	// 动效时长（秒）
	// =========================================================================
	namespace Anim
	{
		constexpr float Fast = 0.18f;
		constexpr float Normal = 0.28f;
		constexpr float Slow = 0.45f;
		constexpr float Cinematic = 0.8f;
		// 缓动常量
		constexpr float EaseOutExponent = 3.0f;
	}
}
