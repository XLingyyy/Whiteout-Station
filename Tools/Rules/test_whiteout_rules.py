from __future__ import annotations

import copy
import random
import unittest

try:
    from .whiteout_rules import (
        WhiteoutSimulator,
        classify_rating,
        load_rules,
        run_route,
        validate_rules,
    )
except ImportError:
    from whiteout_rules import (
        WhiteoutSimulator,
        classify_rating,
        load_rules,
        run_route,
        validate_rules,
    )


class RuleConfigTests(unittest.TestCase):
    def test_config_is_valid(self) -> None:
        self.assertEqual([], validate_rules(load_rules()))

    def test_has_exact_core_content(self) -> None:
        rules = load_rules()
        self.assertEqual(13, len(rules["actions"]))
        self.assertGreaterEqual(len(rules["facts"]), 6)
        self.assertEqual(3, len(rules["routes"]))


class TransactionTests(unittest.TestCase):
    def test_invalid_action_is_atomic(self) -> None:
        sim = WhiteoutSimulator()
        before = copy.deepcopy(sim.state)
        result = sim.apply_action("calibrate_antenna", transaction_id="invalid")
        self.assertFalse(result.committed)
        self.assertEqual("needs_generator", result.reason_code)
        self.assertEqual(before, sim.state)

    def test_duplicate_transaction_commits_once(self) -> None:
        sim = WhiteoutSimulator()
        first = sim.apply_action("investigate_generator_log", transaction_id="same")
        second = sim.apply_action("investigate_generator_log", transaction_id="same")
        self.assertTrue(first.committed)
        self.assertFalse(second.committed)
        self.assertEqual("duplicate_transaction", second.reason_code)
        self.assertEqual(7, sim.state["ap"])
        self.assertEqual(1, len(sim.state["event_log"]))

    def test_resources_never_go_negative(self) -> None:
        sim = WhiteoutSimulator()
        sim.state["resources"]["fuel"] = 0
        before = copy.deepcopy(sim.state)
        result = sim.apply_action("heat_repair_room")
        self.assertFalse(result.committed)
        self.assertEqual(before, sim.state)

    def test_seeded_random_action_sequences_preserve_state_invariants(self) -> None:
        rng = random.Random(20260725)
        action_ids = [action["id"] for action in load_rules()["actions"]]
        food_options = (
            {"player": 1, "gu_heng": 0, "ye_cheng": 0},
            {"player": 0, "gu_heng": 1, "ye_cheng": 0},
            {"player": 0, "gu_heng": 0, "ye_cheng": 1},
            {"player": 1, "gu_heng": 1, "ye_cheng": 0},
            {"player": 1, "gu_heng": 0, "ye_cheng": 1},
            {"player": 0, "gu_heng": 1, "ye_cheng": 1},
        )
        gu_dialogue = (
            {"dialogue_act": "ask"},
            {"dialogue_act": "challenge"},
            {"dialogue_act": "reassure"},
            {
                "dialogue_act": "promise",
                "promise_condition": "reserve_medicine",
            },
            {
                "dialogue_act": "promise",
                "promise_condition": "keep_records",
            },
            {
                "dialogue_act": "promise",
                "promise_condition": "heat_repair_room",
            },
        )
        ye_dialogue = (
            {"dialogue_act": "ask"},
            {"dialogue_act": "challenge"},
            {"dialogue_act": "reassure"},
        )

        for run_index in range(32):
            sim = WhiteoutSimulator()
            for step_index in range(24):
                action_id = rng.choice(action_ids)
                if action_id == "distribute_food":
                    params = dict(rng.choice(food_options))
                elif action_id == "treat_gu_heng":
                    params = {"resource": rng.choice(("medicine", "heat_pack"))}
                elif action_id == "talk_gu_heng":
                    params = dict(rng.choice(gu_dialogue))
                elif action_id == "talk_ye_cheng":
                    params = dict(rng.choice(ye_dialogue))
                else:
                    params = {}
                before = copy.deepcopy(sim.state)
                result = sim.apply_action(
                    action_id,
                    params,
                    f"random-{run_index}-{step_index}",
                )
                if not result.committed:
                    self.assertEqual(before, sim.state)
                self.assertGreaterEqual(sim.state["ap"], 0)
                self.assertLessEqual(sim.state["ap"], 8)
                self.assertTrue(
                    all(value >= 0 for value in sim.state["resources"].values())
                )
                for character in sim.state["characters"].values():
                    for value in character.values():
                        self.assertGreaterEqual(value, 0)
                        self.assertLessEqual(value, 10)
                self.assertEqual(
                    len(sim.state["committed_transactions"]),
                    len(sim.state["event_log"]),
                )


class FlowTests(unittest.TestCase):
    def test_antenna_uses_configured_safe_temperature(self) -> None:
        sim = WhiteoutSimulator()
        threshold = sim.rules["balance"]["thresholds"]["safe_antenna_temperature"]
        sim.state["tasks"]["generator_progress"] = sim.rules["gameplay"][
            "generator_required"
        ]
        sim.state["characters"]["player"]["temperature"] = threshold - 1
        before = copy.deepcopy(sim.state)
        rejected = sim.apply_action("calibrate_antenna", transaction_id="too-cold")
        self.assertFalse(rejected.committed)
        self.assertEqual("player_too_cold", rejected.reason_code)
        self.assertEqual(before, sim.state)

        sim.state["characters"]["player"]["temperature"] = threshold
        allowed = sim.preview_action("calibrate_antenna")
        self.assertTrue(allowed["can_execute"])

    def test_two_ap_action_crosses_crisis_threshold_once(self) -> None:
        sim = WhiteoutSimulator()
        sim.state["ap"] = 5
        sim.state["tasks"]["generator_progress"] = 2
        result = sim.apply_action("calibrate_antenna", transaction_id="cross")
        self.assertTrue(result.committed)
        self.assertTrue(result.crisis_triggered)
        self.assertEqual(3, sim.state["ap"])
        self.assertTrue(sim.state["mid_crisis_triggered"])

    def test_ap_zero_keeps_signal_window(self) -> None:
        sim = WhiteoutSimulator()
        sim.state["ap"] = 2
        sim.state["tasks"]["generator_progress"] = 2
        calibrate = sim.apply_action("calibrate_antenna", transaction_id="last-paid")
        self.assertTrue(calibrate.committed)
        self.assertEqual(0, sim.state["ap"])
        self.assertEqual("post_action_window", sim.state["phase"])
        signal = sim.apply_action("send_signal", transaction_id="free-signal")
        self.assertTrue(signal.committed)
        self.assertTrue(sim.state["tasks"]["signal_sent"])

    def test_two_ap_outdoor_action_ticks_twice(self) -> None:
        sim = WhiteoutSimulator()
        sim.state["tasks"]["generator_progress"] = 2
        before_temp = sim.state["characters"]["player"]["temperature"]
        before_fatigue = sim.state["characters"]["player"]["fatigue"]
        sim.apply_action("calibrate_antenna")
        self.assertAlmostEqual(
            before_temp - 1.2,
            sim.state["characters"]["player"]["temperature"],
        )
        self.assertAlmostEqual(
            before_fatigue - 1.0,
            sim.state["characters"]["player"]["fatigue"],
        )

    def test_endings_are_exclusive(self) -> None:
        sim = WhiteoutSimulator()
        expected = {"task_success", "survival_wait", "cost_uncontrolled", "total_collapse"}
        cases = []
        cases.append(sim.classify_ending())
        sim.state["tasks"]["signal_sent"] = True
        cases.append(sim.classify_ending())
        sim.state["characters"]["gu_heng"]["health"] = 2
        cases.append(sim.classify_ending())
        sim.state["tasks"]["signal_sent"] = False
        sim.state["tasks"]["generator_progress"] = 0
        sim.state["resources"]["fuel"] = 0
        cases.append(sim.classify_ending())
        self.assertTrue(set(cases).issubset(expected))
        self.assertEqual(4, len(set(cases)))


class KnowledgeAndAgentTests(unittest.TestCase):
    def test_initial_contexts_do_not_leak_other_npc_secret(self) -> None:
        sim = WhiteoutSimulator()
        gu_context = sim.build_agent_context("gu_heng")
        ye_context = sim.build_agent_context("ye_cheng")
        self.assertNotIn("FACT_HEAT_PACK", gu_context["allowed_fact_ids"])
        self.assertNotIn("FACT_RELAY_COMPATIBILITY", ye_context["allowed_fact_ids"])

    def test_agent_fact_violation_forces_rejection(self) -> None:
        valid, reason = WhiteoutSimulator.validate_agent_response(
            {
                "utterance": "我知道那个保温包。",
                "referenced_fact_ids": ["FACT_HEAT_PACK"],
            },
            ["FACT_HAND_INJURY"],
        )
        self.assertFalse(valid)
        self.assertEqual("fact_permission_violation", reason)

    def test_agent_cannot_change_rules(self) -> None:
        valid, reason = WhiteoutSimulator.validate_agent_response(
            {"utterance": "修好了。", "task_progress": 2, "referenced_fact_ids": []}, []
        )
        self.assertFalse(valid)
        self.assertEqual("model_attempted_rule_change", reason)

    def test_promise_is_recognized_and_settled_once(self) -> None:
        sim = WhiteoutSimulator()
        sim.apply_action("investigate_generator_log", transaction_id="records-context")
        sim.apply_action(
            "talk_gu_heng",
            {"dialogue_act": "promise", "condition": "keep_records"},
            "promise-action",
        )
        self.assertEqual(1, len(sim.state["promises"]))
        sim.state["flags"]["records_preserved"] = True
        first = sim.end_game()
        trust = sim.state["characters"]["gu_heng"]["trust"]
        second = sim.end_game()
        self.assertTrue(sim.state["promises"][0]["fulfilled"])
        self.assertEqual(trust, sim.state["characters"]["gu_heng"]["trust"])
        self.assertEqual(first["score"], second["score"])


class DialogueIntentTests(unittest.TestCase):
    def test_unknown_dialogue_acts_are_rejected_atomically(self) -> None:
        for action_id in ("talk_gu_heng", "talk_ye_cheng"):
            with self.subTest(action_id=action_id):
                sim = WhiteoutSimulator()
                params = {"dialogue_act": "command"}
                before = copy.deepcopy(sim.state)
                preview = sim.preview_action(action_id, params)
                result = sim.apply_action(action_id, params, f"unknown-{action_id}")
                self.assertFalse(preview["can_execute"])
                self.assertEqual("dialogue_act_unavailable", preview["reason_code"])
                self.assertFalse(result.committed)
                self.assertEqual("dialogue_act_unavailable", result.reason_code)
                self.assertEqual(before, sim.state)

    def test_non_promise_act_cannot_carry_a_promise_condition(self) -> None:
        for action_id in ("talk_gu_heng", "talk_ye_cheng"):
            for condition in ("keep_records", "   "):
                with self.subTest(action_id=action_id, condition=condition):
                    sim = WhiteoutSimulator()
                    params = {
                        "dialogue_act": "ask",
                        "promise_condition": condition,
                    }
                    before = copy.deepcopy(sim.state)
                    preview = sim.preview_action(action_id, params)
                    result = sim.apply_action(
                        action_id,
                        params,
                        f"mixed-{action_id}-{condition!r}",
                    )
                    self.assertFalse(preview["can_execute"])
                    self.assertEqual(
                        "invalid_promise_condition",
                        preview["reason_code"],
                    )
                    self.assertFalse(result.committed)
                    self.assertEqual(
                        "invalid_promise_condition",
                        result.reason_code,
                    )
                    self.assertEqual(before, sim.state)

    def test_ye_cheng_promise_is_rejected_atomically(self) -> None:
        sim = WhiteoutSimulator()
        params = {
            "dialogue_act": "promise",
            "promise_condition": "keep_records",
        }
        before = copy.deepcopy(sim.state)

        preview = sim.preview_action("talk_ye_cheng", params)
        result = sim.apply_action("talk_ye_cheng", params, "ye-promise")

        self.assertFalse(preview["can_execute"])
        self.assertEqual("dialogue_act_unavailable", preview["reason_code"])
        self.assertFalse(result.committed)
        self.assertEqual("dialogue_act_unavailable", result.reason_code)
        self.assertEqual(before, sim.state)

    def test_gu_heng_invalid_promise_condition_is_rejected_atomically(self) -> None:
        sim = WhiteoutSimulator()
        params = {
            "dialogue_act": "promise",
            "promise_condition": "unconfigured_condition",
        }
        before = copy.deepcopy(sim.state)

        preview = sim.preview_action("talk_gu_heng", params)
        result = sim.apply_action("talk_gu_heng", params, "invalid-gu-promise")

        self.assertFalse(preview["can_execute"])
        self.assertEqual("invalid_promise_condition", preview["reason_code"])
        self.assertFalse(result.committed)
        self.assertEqual("invalid_promise_condition", result.reason_code)
        self.assertEqual(before, sim.state)

    def test_duplicate_gu_heng_promise_is_rejected_atomically(self) -> None:
        sim = WhiteoutSimulator()
        sim.apply_action("investigate_generator_log", transaction_id="records-context")
        params = {
            "dialogue_act": "promise",
            "promise_condition": "keep_records",
        }
        first = sim.apply_action("talk_gu_heng", params, "first-gu-promise")
        before_duplicate = copy.deepcopy(sim.state)

        preview = sim.preview_action("talk_gu_heng", params)
        duplicate = sim.apply_action("talk_gu_heng", params, "duplicate-gu-promise")

        self.assertTrue(first.committed)
        self.assertEqual(1, len(sim.state["promises"]))
        self.assertFalse(preview["can_execute"])
        self.assertEqual("duplicate_promise", preview["reason_code"])
        self.assertFalse(duplicate.committed)
        self.assertEqual("duplicate_promise", duplicate.reason_code)
        self.assertEqual(before_duplicate, sim.state)

    def test_intents_unlock_from_story_context(self) -> None:
        sim = WhiteoutSimulator()
        self.assertTrue(sim.preview_action("talk_ye_cheng", {"dialogue_act": "ask"})["can_execute"])
        self.assertFalse(
            sim.preview_action("talk_ye_cheng", {"dialogue_act": "challenge"})[
                "can_execute"
            ]
        )
        self.assertFalse(
            sim.preview_action("talk_ye_cheng", {"dialogue_act": "reassure"})[
                "can_execute"
            ]
        )
        self.assertTrue(
            sim.preview_action("talk_gu_heng", {"dialogue_act": "reassure"})[
                "can_execute"
            ]
        )
        self.assertFalse(
            sim.preview_action(
                "talk_gu_heng",
                {
                    "dialogue_act": "promise",
                    "promise_condition": "heat_repair_room",
                },
            )["can_execute"]
        )

        sim.apply_action("investigate_generator_log", transaction_id="stage-log")
        self.assertTrue(
            sim.preview_action("talk_gu_heng", {"dialogue_act": "challenge"})[
                "can_execute"
            ]
        )
        self.assertTrue(
            sim.preview_action(
                "talk_gu_heng",
                {
                    "dialogue_act": "promise",
                    "promise_condition": "keep_records",
                },
            )["can_execute"]
        )
        self.assertFalse(
            sim.preview_action(
                "talk_gu_heng",
                {
                    "dialogue_act": "promise",
                    "promise_condition": "reserve_medicine",
                },
            )["can_execute"]
        )

        sim = WhiteoutSimulator()
        sim.apply_action(
            "talk_ye_cheng",
            {"dialogue_act": "ask"},
            "stage-ask-ye",
        )
        self.assertTrue(
            sim.preview_action("talk_ye_cheng", {"dialogue_act": "challenge"})[
                "can_execute"
            ]
        )
        for condition in ("reserve_medicine", "heat_repair_room"):
            self.assertTrue(
                sim.preview_action(
                    "talk_gu_heng",
                    {
                        "dialogue_act": "promise",
                        "promise_condition": condition,
                    },
                )["can_execute"]
            )

    def test_dialogue_intents_apply_deterministic_post_base_deltas(self) -> None:
        cases = [
            ("talk_ye_cheng", "ask", None, 6.4, 4.4, 0),
            ("talk_ye_cheng", "challenge", None, 6.1, 4.7, 0),
            ("talk_ye_cheng", "reassure", None, 6.7, 4.0, 0),
            ("talk_gu_heng", "ask", None, 3.2, 7.6, 0),
            ("talk_gu_heng", "challenge", None, 2.9, 7.9, 0),
            ("talk_gu_heng", "reassure", None, 3.5, 7.2, 0),
            (
                "talk_gu_heng",
                "promise",
                "keep_records",
                3.4,
                7.4,
                1,
            ),
        ]
        for action_id, dialogue_act, condition, trust, pressure, promises in cases:
            with self.subTest(action=action_id, intent=dialogue_act):
                sim = WhiteoutSimulator()
                if action_id == "talk_ye_cheng" and dialogue_act == "challenge":
                    sim.state["flags"]["heat_pack_revealed"] = True
                elif action_id == "talk_ye_cheng" and dialogue_act == "reassure":
                    sim.state["mid_crisis_triggered"] = True
                elif action_id == "talk_gu_heng" and dialogue_act in {
                    "challenge",
                    "promise",
                }:
                    sim.state["player_knowledge"][
                        "FACT_FORCED_RESTART_SUSPICION"
                    ] = "suspected"
                params = {"dialogue_act": dialogue_act}
                if condition is not None:
                    params["promise_condition"] = condition

                result = sim.apply_action(action_id, params)
                character_id = (
                    "gu_heng" if action_id == "talk_gu_heng" else "ye_cheng"
                )
                character = sim.state["characters"][character_id]

                self.assertTrue(result.committed)
                self.assertAlmostEqual(trust, character["trust"])
                self.assertAlmostEqual(pressure, character["pressure"])
                self.assertEqual(promises, len(sim.state["promises"]))

    def test_evidence_backed_gu_heng_challenge_uses_special_modifier(self) -> None:
        results: dict[str, tuple[float, float]] = {}
        for dialogue_act in ("ask", "challenge"):
            sim = WhiteoutSimulator()
            sim.apply_action("investigate_generator_log")
            sim.apply_action("inspect_control_cabinet")
            result = sim.apply_action(
                "talk_gu_heng",
                {"dialogue_act": dialogue_act},
            )
            gu_heng = sim.state["characters"]["gu_heng"]
            self.assertTrue(result.committed)
            results[dialogue_act] = (gu_heng["trust"], gu_heng["pressure"])

        self.assertAlmostEqual(4.3, results["ask"][0])
        self.assertAlmostEqual(7.5, results["ask"][1])
        self.assertAlmostEqual(4.5, results["challenge"][0])
        self.assertAlmostEqual(7.7, results["challenge"][1])


class RouteAndScoreTests(unittest.TestCase):
    def test_all_routes_succeed_in_expected_ranges(self) -> None:
        rules = load_rules()
        expected = {
            "medical_cooperation": {
                "ap": 0,
                "score": 76.76,
                "dialogue": {
                    "dialogue_act": "promise",
                    "promise_condition": "heat_repair_room",
                },
            },
            "technical_replacement": {
                "ap": 0,
                "score": 72.02,
                "dialogue": {"dialogue_act": "challenge"},
            },
            "forced_quick_repair": {
                "ap": 2,
                "score": 68.31,
                "ending": "cost_uncontrolled",
                "dialogue": None,
            },
        }
        for route_id, route in rules["routes"].items():
            with self.subTest(route=route_id):
                output = run_route(WhiteoutSimulator(rules), route_id)
                self.assertEqual(
                    expected[route_id].get("ending", "task_success"),
                    output["ending"],
                )
                self.assertEqual(expected[route_id]["ap"], output["steps"][-1]["ap"])
                self.assertAlmostEqual(
                    expected[route_id]["score"],
                    output["score"]["total"],
                    places=2,
                )
                low, high = route["expected_score_range"]
                self.assertGreaterEqual(output["score"]["total"], low)
                self.assertLessEqual(output["score"]["total"], high)
                dialogue_steps = [
                    step
                    for step in output["steps"]
                    if step["action"] == "talk_gu_heng"
                ]
                if expected[route_id]["dialogue"] is None:
                    self.assertEqual([], dialogue_steps)
                else:
                    self.assertEqual(
                        expected[route_id]["dialogue"],
                        dialogue_steps[0]["params"],
                    )

    def test_all_four_endings_are_reachable_through_committed_actions(self) -> None:
        wait = WhiteoutSimulator()
        self.assertEqual("survival_wait", wait.end_game()["ending"])

        cost = WhiteoutSimulator()
        cost.apply_action("investigate_generator_log")
        self.assertTrue(cost.apply_action("forced_self_repair").committed)
        for index in range(2):
            result = cost.apply_action(
                "talk_gu_heng",
                {"dialogue_act": "challenge"},
                f"cost-challenge-{index}",
            )
            self.assertTrue(result.committed)
        self.assertEqual("cost_uncontrolled", cost.end_game()["ending"])

        collapse = WhiteoutSimulator()
        collapse.apply_action("investigate_generator_log")
        for index in range(2):
            collapse.apply_action(
                "talk_gu_heng",
                {"dialogue_act": "challenge"},
                f"collapse-challenge-{index}",
            )
        collapse.apply_action("heat_medical_room")
        collapse.apply_action("heat_repair_room")
        collapse.apply_action("talk_ye_cheng", {"dialogue_act": "ask"})
        collapse.apply_action(
            "distribute_food",
            {"player": 1, "gu_heng": 0, "ye_cheng": 0},
        )
        self.assertTrue(collapse.apply_action("inspect_control_cabinet").committed)
        self.assertEqual(0, collapse.state["ap"])
        self.assertEqual("total_collapse", collapse.end_game()["ending"])

        success = run_route(WhiteoutSimulator(), "medical_cooperation")
        self.assertEqual("task_success", success["ending"])

    def test_legacy_routes_receive_deterministic_dialogue_defaults(self) -> None:
        rules = load_rules()
        for route_id in ("medical_cooperation", "technical_replacement"):
            for step in rules["routes"][route_id]["steps"]:
                if step["action"] == "talk_gu_heng":
                    step.pop("params", None)

        medical = run_route(WhiteoutSimulator(rules), "medical_cooperation")
        technical = run_route(WhiteoutSimulator(rules), "technical_replacement")

        medical_talk = next(
            step for step in medical["steps"] if step["action"] == "talk_gu_heng"
        )
        technical_talk = next(
            step for step in technical["steps"] if step["action"] == "talk_gu_heng"
        )
        self.assertEqual(
            {
                "dialogue_act": "promise",
                "promise_condition": "heat_repair_room",
            },
            medical_talk["params"],
        )
        self.assertEqual(
            {"dialogue_act": "challenge"},
            technical_talk["params"],
        )

    def test_routes_are_deterministic(self) -> None:
        first = run_route(WhiteoutSimulator(), "technical_replacement")
        second = run_route(WhiteoutSimulator(), "technical_replacement")
        self.assertEqual(first["score"], second["score"])
        self.assertEqual(first["ending"], second["ending"])

    def test_critical_hoarded_medicine_is_capped(self) -> None:
        sim = WhiteoutSimulator()
        sim.state["characters"]["gu_heng"]["health"] = 20
        sim.state["resources"]["medicine"] = 1
        score_with_medicine = sim.calculate_score()["breakdown"]["effective_reserves"]
        sim.state["resources"]["medicine"] = 0
        score_without_medicine = sim.calculate_score()["breakdown"]["effective_reserves"]
        self.assertLessEqual(score_with_medicine - score_without_medicine, 1.25)

    def test_score_is_clamped(self) -> None:
        sim = WhiteoutSimulator()
        score = sim.calculate_score()
        self.assertGreaterEqual(score["total"], 0)
        self.assertLessEqual(score["total"], 100)

    def test_rating_boundaries_have_no_fractional_gaps(self) -> None:
        ratings = load_rules()["score"]["ratings"]
        cases = [
            (0, "D"),
            (59.99, "D"),
            (60, "C"),
            (69.99, "C"),
            (70, "B"),
            (79.99, "B"),
            (80, "A"),
            (89.99, "A"),
            (90, "S"),
            (100, "S"),
        ]
        for total, expected in cases:
            with self.subTest(total=total):
                self.assertEqual(expected, classify_rating(total, ratings))


if __name__ == "__main__":
    unittest.main()
