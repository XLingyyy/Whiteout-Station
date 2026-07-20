from __future__ import annotations

import copy
import unittest

from whiteout_rules import WhiteoutSimulator, load_rules, run_route, validate_rules


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


class FlowTests(unittest.TestCase):
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
        self.assertEqual(before_temp - 12, sim.state["characters"]["player"]["temperature"])
        self.assertEqual(before_fatigue - 10, sim.state["characters"]["player"]["fatigue"])

    def test_endings_are_exclusive(self) -> None:
        sim = WhiteoutSimulator()
        expected = {"task_success", "survival_wait", "cost_uncontrolled", "total_collapse"}
        cases = []
        cases.append(sim.classify_ending())
        sim.state["tasks"]["signal_sent"] = True
        cases.append(sim.classify_ending())
        sim.state["characters"]["gu_heng"]["health"] = 20
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


class RouteAndScoreTests(unittest.TestCase):
    def test_all_routes_succeed_in_expected_ranges(self) -> None:
        rules = load_rules()
        for route_id, route in rules["routes"].items():
            with self.subTest(route=route_id):
                output = run_route(WhiteoutSimulator(rules), route_id)
                self.assertEqual("task_success", output["ending"])
                low, high = route["expected_score_range"]
                self.assertGreaterEqual(output["score"]["total"], low)
                self.assertLessEqual(output["score"]["total"], high)

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


if __name__ == "__main__":
    unittest.main()
