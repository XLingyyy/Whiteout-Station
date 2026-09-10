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
	namespace V17
	{
		constexpr int32 UIRevision = 1;
		inline const FLinearColor Surface(14.f/255, 20.f/255, 25.f/255, .94f);
		inline const FLinearColor SurfaceRaised(23.f/255, 31.f/255, 37.f/255, .95f);
		inline const FLinearColor Text(237.f/255, 240.f/255, 235.f/255);
		inline const FLinearColor Secondary(165.f/255, 176.f/255, 182.f/255);
		inline const FLinearColor Stroke(57.f/255, 67.f/255, 74.f/255);
		inline const FLinearColor Divider(48.f/255, 58.f/255, 64.f/255);
		inline const FLinearColor Attention(223.f/255, 171.f/255, 103.f/255);
		inline const FLinearColor Critical(237.f/255, 138.f/255, 123.f/255);
		constexpr float Radius = 9;
		constexpr float FadeIn = .18f;
		constexpr float FadeOut = .12f;
		constexpr float TutorialBlur = 14;
	}
	namespace V15
	{
		inline const FLinearColor Surface(0.025f, 0.033f, 0.043f, 0.94f);
		inline const FLinearColor Stroke(0.65f, 0.70f, 0.75f, 0.18f);
		inline const FLinearColor Text(0.91f, 0.93f, 0.95f);
		inline const FLinearColor Attention(0.95f, 0.75f, 0.3f);
		inline const FLinearColor Critical(1.0f, 0.3f, 0.3f);
		constexpr float Radius = 6.0f;
		constexpr float FadeIn = 0.12f;
		constexpr float FadeOut = 0.10f;
	}
	// =========================================================================
	// 颜色 — Surface
	// =========================================================================
	namespace Color
	{
		// 深面板：ESC、证据板主面板 — 中性黑
		inline const FLinearColor SurfaceDeep = V17::SurfaceRaised;
		// 标准面板：HUD、人物卡、轮盘卡 — 中性黑
		inline const FLinearColor SurfacePanel = V17::Surface;
		// 紧凑表面：局部信息背衬、底部提示
		inline const FLinearColor SurfaceCompact = V17::Surface;
		// 悬停表面：鼠标悬停、键盘焦点
		inline const FLinearColor SurfaceHover = V17::SurfaceRaised;
		// 对话条深底
		inline const FLinearColor SurfaceDialogue = V17::Surface;
		// 结算/全屏暗底
		inline const FLinearColor SurfaceFullscreen(0.004f, 0.004f, 0.004f, 1.0f);
		// 预览面板底
		inline const FLinearColor SurfacePreview = V17::SurfaceRaised;
		// 证据过滤/详情子面板
		inline const FLinearColor SurfaceFilter = V17::Surface;
		// 对话输入框底
		inline const FLinearColor SurfaceInput = V17::Surface;
		inline const FLinearColor SurfaceInputFocused = V17::SurfaceRaised;

		// 描边 — 中性白
		inline const FLinearColor StrokeHairline = V17::Stroke;
		inline const FLinearColor StrokeHairlineSubtle(0.86f, 0.86f, 0.86f, 0.06f);
		inline const FLinearColor StrokeFocus = V17::Attention;
		inline const FLinearColor StrokeDivider = V17::Divider;

		// 文字
		inline const FLinearColor TextPrimary = V17::Text;
		inline const FLinearColor TextSecondary = V17::Secondary;
		inline const FLinearColor TextMuted(0.467f, 0.518f, 0.557f, 1.0f);
		inline const FLinearColor TextCinematicWarm(0.82f, 0.92f, 1.0f, 1.0f);

		// 强调色 — 橙色纪律：仅 AP / 警告 / 当前交互目标
		inline const FLinearColor AccentAction = V17::Attention;
		inline const FLinearColor AccentWarning = V17::Critical;
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
		inline const FLinearColor ProgressBarBackground(0.025f, 0.025f, 0.025f, 0.65f);

		// 按钮状态
		inline const FLinearColor ButtonNormal = V17::Surface;
		inline const FLinearColor ButtonHover = V17::SurfaceRaised;
		inline const FLinearColor ButtonPressed = V17::Stroke;
		inline const FLinearColor ButtonDisabled = V17::Surface;

		// 对话意图按钮底
		inline const FLinearColor DialogueChoiceNormal = V17::Surface;
		inline const FLinearColor DialogueChoiceHover = V17::SurfaceRaised;

		// 滑块
		inline const FLinearColor SliderBar(0.05f, 0.05f, 0.05f, 1.0f);
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
