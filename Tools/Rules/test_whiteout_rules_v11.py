from __future__ import annotations

import copy
import unittest

try:
    from .whiteout_rules_v11 import (
        RATING_ORDER,
        WhiteoutSimulatorV11,
        load_rules,
        run_route,
        validate_rules,
    )
except ImportError:
    from whiteout_rules_v11 import (
        RATING_ORDER,
        WhiteoutSimulatorV11,
        load_rules,
        run_route,
        validate_rules,
    )


def started(zone: str = "repair_room") -> WhiteoutSimulatorV11:
    simulator = WhiteoutSimulatorV11()
    result = simulator.start_phase(zone)
    if not result["committed"]:
        raise AssertionError(result)
    return simulator


class V11ConfigTests(unittest.TestCase):
    def test_schema_four_config_is_valid_and_v10_shape_is_not_reused(self) -> None:
        rules = load_rules()
        self.assertEqual([], validate_rules(rules))
        self.assertEqual(4, rules["schema_version"])
        self.assertEqual(4, rules["gameplay"]["action_points_per_phase"])
        self.assertEqual(["morning", "afternoon", "dusk"], rules["gameplay"]["phases"])
        self.assertEqual(
            {
                "task_quality": 30,
                "people": 30,
                "effective_reserves": 15,
                "social_stability": 15,
                "information_responsibility": 10,
            },
            rules["score"]["weights"],
        )

    def test_m0_contains_three_success_and_two_failure_routes(self) -> None:
        routes = load_rules()["routes"].values()
        self.assertGreaterEqual(
            sum(1 for route in routes if route["expected_success"]), 3
        )
        self.assertGreaterEqual(
            sum(1 for route in routes if not route["expected_success"]), 2
        )


class PhaseTests(unittest.TestCase):
    def test_heating_is_single_zone_locked_and_consumes_one_fuel(self) -> None:
        simulator = WhiteoutSimulatorV11()
        before_fuel = simulator.state["resources"]["fuel"]
        first = simulator.start_phase("medical_room")
        locked_snapshot = copy.deepcopy(simulator.state)
        second = simulator.start_phase("repair_room")

        self.assertTrue(first["committed"])
        self.assertEqual(before_fuel - 1, simulator.state["resources"]["fuel"])
        self.assertFalse(second["committed"])
        self.assertEqual("heating_locked", second["reason_code"])
        self.assertEqual(locked_snapshot, simulator.state)

    def test_unused_ap_never_carries_to_next_phase(self) -> None:
        simulator = started("control_room")
        self.assertTrue(simulator.apply_action("investigate_generator_log").committed)
        summary = simulator.settle_phase()

        self.assertTrue(summary["committed"])
        self.assertEqual(3, summary["unused_ap_discarded"])
        self.assertEqual("afternoon", simulator.state["phase"])
        self.assertEqual(4, simulator.state["phase_ap"])
        self.assertFalse(simulator.state["phase_started"])

    def test_settlement_order_and_phase_event_are_recorded_once(self) -> None:
        simulator = started("kitchen")
        summary = simulator.settle_phase()
        event_count = len(simulator.state["event_log"])
        rejected = simulator.settle_phase()

        self.assertEqual(
            [1, 2, 3, 4, 5, 6],
            [step["order"] for step in summary["ordered_steps"]],
        )
        self.assertEqual("backup_power_saving", summary["event"]["id"])
        self.assertFalse(rejected["committed"])
        self.assertEqual("phase_not_started", rejected["reason_code"])
        self.assertEqual(event_count, len(simulator.state["event_log"]))

    def test_each_new_phase_starts_with_exactly_four_ap(self) -> None:
        simulator = WhiteoutSimulatorV11()
        for phase, zone in zip(
            ("morning", "afternoon", "dusk"),
            ("kitchen", "repair_room", "control_room"),
        ):
            self.assertEqual(phase, simulator.state["phase"])
            self.assertEqual(4, simulator.state["phase_ap"])
            simulator.start_phase(zone)
            simulator.settle_phase()
        self.assertTrue(simulator.state["window_closed"])
        self.assertEqual(0, simulator.state["phase_ap"])


class DynamicCostTests(unittest.TestCase):
    def test_tired_cold_and_injury_modifiers_are_visible_and_capped(self) -> None:
        simulator = started("control_room")
        simulator.state["tasks"]["generator_progress"] = 2
        player = simulator.state["characters"]["player"]
        player["stamina"] = 1
        player["temperature"] = 5.0
        player["injuries"] = ["right_hand_restricted"]

        preview = simulator.build_action_preview("calibrate_antenna")
        sources = {item["source"] for item in preview["modifiers"]}
        self.assertEqual(
            {
                "executor_tired",
                "executor_cold",
                "relevant_injury_restricted",
            },
            sources,
        )
        self.assertEqual(5, preview["raw_ap"])
        self.assertEqual(4, preview["final_ap"])
        self.assertEqual("high_risk", preview["readiness"])

    def test_heated_room_cancels_cold_fine_motor_penalty(self) -> None:
        simulator = started("repair_room")
        simulator.state["characters"]["player"]["temperature"] = 5.0
        preview = simulator.build_action_preview("inspect_control_cabinet")
        self.assertEqual(1, preview["final_ap"])
        self.assertIn(
            "heated_room_cancels_cold",
            {item["source"] for item in preview["modifiers"]},
        )

    def test_collaborator_reduces_cost_to_minimum_one(self) -> None:
        simulator = started("control_room")
        simulator.state["tasks"]["generator_progress"] = 2
        solo = simulator.build_action_preview("calibrate_antenna")
        assisted = simulator.build_action_preview(
            "calibrate_antenna", {"collaborator": "ye_cheng"}
        )
        self.assertEqual(2, solo["final_ap"])
        self.assertEqual(1, assisted["final_ap"])

    def test_relay_reduces_risky_repair_cost(self) -> None:
        simulator = started("repair_room")
        simulator.state["characters"]["gu_heng"]["stamina"] = 1
        simulator.state["resources"]["replacement_relay"] = 1
        without_relay = simulator.build_action_preview(
            "repair_generator", {"collaborator": "player", "force": True}
        )
        with_relay = simulator.build_action_preview(
            "repair_generator",
            {
                "collaborator": "player",
                "force": True,
                "use_relay": True,
            },
        )
        self.assertEqual(2, without_relay["final_ap"])
        self.assertEqual(1, with_relay["final_ap"])

    def test_exhaustion_and_hypothermia_block_unforced_work(self) -> None:
        simulator = started("control_room")
        simulator.state["tasks"]["generator_progress"] = 2
        simulator.state["characters"]["player"]["stamina"] = 0
        exhausted = simulator.build_action_preview("calibrate_antenna")
        self.assertFalse(exhausted["can_execute"])
        self.assertEqual("executor_exhausted", exhausted["reason_code"])

        simulator.state["characters"]["player"]["stamina"] = 2
        simulator.state["characters"]["player"]["temperature"] = 3.0
        hypothermic = simulator.build_action_preview("calibrate_antenna")
        self.assertFalse(hypothermic["can_execute"])
        self.assertEqual("executor_hypothermic", hypothermic["reason_code"])

    def test_zero_ap_signal_is_the_only_cost_floor_exception(self) -> None:
        simulator = started("control_room")
        simulator.state["tasks"].update(
            {"generator_progress": 2, "antenna_calibration": 1}
        )
        signal = simulator.build_action_preview("send_signal")
        self.assertTrue(signal["can_execute"])
        self.assertEqual(0, signal["final_ap"])

    def test_preview_and_committed_cost_are_identical(self) -> None:
        simulator = started("control_room")
        preview = simulator.build_action_preview("investigate_generator_log")
        result = simulator.apply_action(
            "investigate_generator_log", transaction_id="same-cost"
        )
        self.assertTrue(result.committed)
        self.assertEqual(preview["final_ap"], result.cost["final_ap"])
        self.assertEqual(4 - preview["final_ap"], result.ap_after)
        action_event = simulator.state["event_log"][-1]
        self.assertEqual(preview["final_ap"], action_event["cost"]["final_ap"])


class AtomicityTests(unittest.TestCase):
    def test_invalid_action_is_atomic(self) -> None:
        simulator = started("medical_room")
        before = copy.deepcopy(simulator.state)
        rejected = simulator.apply_action(
            "treat_character",
            {"target": "player", "method": "full"},
            "not-needed",
        )
        self.assertFalse(rejected.committed)
        self.assertEqual("treatment_not_needed", rejected.reason_code)
        self.assertEqual(before, simulator.state)

    def test_duplicate_transaction_only_commits_once(self) -> None:
        simulator = started("control_room")
        first = simulator.apply_action(
            "investigate_generator_log", transaction_id="duplicate"
        )
        before_duplicate = copy.deepcopy(simulator.state)
        duplicate = simulator.apply_action(
            "investigate_generator_log", transaction_id="duplicate"
        )
        self.assertTrue(first.committed)
        self.assertFalse(duplicate.committed)
        self.assertEqual("duplicate_transaction", duplicate.reason_code)
        self.assertEqual(before_duplicate, simulator.state)

    def test_all_configured_routes_preserve_non_negative_resources(self) -> None:
        for route_id in load_rules()["routes"]:
            with self.subTest(route=route_id):
                simulator = WhiteoutSimulatorV11()
                run_route(simulator, route_id)
                self.assertTrue(
                    all(value >= 0 for value in simulator.state["resources"].values())
                )


class FoodAndRecoveryTests(unittest.TestCase):
    def test_each_character_can_receive_food_and_restore_stamina(self) -> None:
        for character_id in ("player", "gu_heng", "ye_cheng"):
            with self.subTest(character=character_id):
                simulator = started("kitchen")
                simulator.state["characters"][character_id]["stamina"] = (
                    1 if character_id == "player" else 0
                )
                result = simulator.apply_action(
                    "distribute_food",
                    {"recipients": [character_id], "meal_type": "hot"},
                )
                self.assertTrue(result.committed)
                self.assertEqual(
                    1, simulator.state["characters"][character_id]["stamina"]
                )

    def test_hot_meal_has_temperature_and_pressure_benefits(self) -> None:
        cold = started("kitchen")
        hot = started("kitchen")
        for simulator in (cold, hot):
            simulator.state["characters"]["gu_heng"]["stamina"] = 0
        cold.apply_action(
            "distribute_food",
            {"recipients": ["gu_heng"], "meal_type": "cold"},
        )
        hot.apply_action(
            "distribute_food",
            {"recipients": ["gu_heng"], "meal_type": "hot"},
        )
        self.assertGreater(
            hot.state["characters"]["gu_heng"]["temperature"],
            cold.state["characters"]["gu_heng"]["temperature"],
        )
        self.assertLess(
            hot.state["characters"]["gu_heng"]["pressure"],
            cold.state["characters"]["gu_heng"]["pressure"],
        )

    def test_hot_meal_requires_heated_intact_kitchen(self) -> None:
        simulator = started("repair_room")
        before = copy.deepcopy(simulator.state)
        result = simulator.apply_action(
            "distribute_food",
            {"recipients": ["player"], "meal_type": "hot"},
        )
        self.assertFalse(result.committed)
        self.assertEqual("hot_meal_unavailable", result.reason_code)
        self.assertEqual(before, simulator.state)

    def test_heated_rest_restores_stamina_unheated_rest_only_pressure(self) -> None:
        heated = started("repair_room")
        unheated = started("repair_room")
        for simulator in (heated, unheated):
            simulator.state["characters"]["player"]["stamina"] = 0
        heated.apply_action(
            "rest", {"target": "player", "location": "repair_room"}
        )
        unheated.apply_action(
            "rest", {"target": "player", "location": "control_room"}
        )
        self.assertEqual(1, heated.state["characters"]["player"]["stamina"])
        self.assertEqual(0, unheated.state["characters"]["player"]["stamina"])
        self.assertLess(unheated.state["characters"]["player"]["pressure"], 4.0)


class MedicalAndInjuryTests(unittest.TestCase):
    def test_full_treatment_is_generic_and_consumes_medicine(self) -> None:
        simulator = started("medical_room")
        player = simulator.state["characters"]["player"]
        player["injuries"] = ["right_hand_restricted"]
        result = simulator.apply_action(
            "treat_character",
            {
                "target": "player",
                "method": "full",
                "collaborator": "player",
            },
        )
        self.assertTrue(result.committed)
        self.assertEqual([], player["injuries"])
        self.assertEqual(0, simulator.state["resources"]["medicine"])

    def test_bandage_prevents_next_worsening_but_keeps_restriction(self) -> None:
        simulator = started("medical_room")
        simulator.state["characters"]["gu_heng"]["stamina"] = 2
        bandage = simulator.apply_action(
            "treat_character",
            {
                "target": "gu_heng",
                "method": "bandage",
                "collaborator": "gu_heng",
            },
        )
        self.assertTrue(bandage.committed)
        self.assertEqual(
            ["right_hand_restricted"],
            simulator.state["characters"]["gu_heng"]["injuries"],
        )
        repair = simulator.apply_action(
            "repair_generator",
            {"collaborator": "player", "force": True},
        )
        self.assertTrue(repair.committed)
        gu_heng = simulator.state["characters"]["gu_heng"]
        self.assertEqual(0, gu_heng["injury_worsening_marks"])
        self.assertEqual(0, gu_heng["bandage_protection"])

    def test_heat_pack_waives_one_penalty_without_curing_injury(self) -> None:
        simulator = started("medical_room")
        simulator.state["flags"]["heat_pack_revealed"] = True
        simulator.state["characters"]["gu_heng"]["stamina"] = 2
        support = simulator.apply_action(
            "treat_character",
            {
                "target": "gu_heng",
                "method": "heat_pack",
                "collaborator": "gu_heng",
            },
        )
        self.assertTrue(support.committed)
        preview = simulator.build_action_preview(
            "repair_generator", {"collaborator": "player", "force": True}
        )
        self.assertIn(
            "temporary_support_cancels_injury",
            {item["source"] for item in preview["modifiers"]},
        )
        repair = simulator.apply_action(
            "repair_generator",
            {"collaborator": "player", "force": True},
        )
        self.assertTrue(repair.committed)
        self.assertEqual(
            ["right_hand_restricted"],
            simulator.state["characters"]["gu_heng"]["injuries"],
        )
        self.assertEqual(
            0, simulator.state["characters"]["gu_heng"]["injury_worsening_marks"]
        )

    def test_second_unhandled_injured_repair_becomes_critical(self) -> None:
        simulator = started("repair_room")
        simulator.state["characters"]["gu_heng"]["stamina"] = 2
        for index in range(2):
            result = simulator.apply_action(
                "repair_generator",
                {"collaborator": "player", "force": True},
                f"risky-{index}",
            )
            self.assertTrue(result.committed)
        gu_heng = simulator.state["characters"]["gu_heng"]
        self.assertEqual(["right_hand_critical"], gu_heng["injuries"])
        self.assertEqual(2, gu_heng["injury_worsening_marks"])
        unavailable = simulator.build_action_preview(
            "repair_generator", {"collaborator": "player"}
        )
        self.assertFalse(unavailable["can_execute"])


class NpcAndModelBoundaryTests(unittest.TestCase):
    def test_all_deterministic_stances_are_reachable(self) -> None:
        simulator = started("control_room")
        withhold = simulator.resolve_npc_stance("gu_heng", "status_update")
        conditional = simulator.resolve_npc_stance(
            "gu_heng", "repair_generator"
        )

        simulator.state["characters"]["gu_heng"]["stamina"] = 2
        simulator.state["characters"]["gu_heng"]["trust"] = 6.0
        volunteer = simulator.resolve_npc_stance(
            "gu_heng", "repair_generator"
        )

        simulator.state["characters"]["gu_heng"]["trust"] = 4.5
        simulator.state["heating"]["current_zone"] = "repair_room"
        accept = simulator.resolve_npc_stance("gu_heng", "repair_generator")

        simulator.state["characters"]["gu_heng"]["stamina"] = 0
        refuse = simulator.resolve_npc_stance("gu_heng", "repair_generator")
        self.assertEqual(
            {
                "withhold",
                "conditional_accept",
                "volunteer",
                "accept",
                "refuse",
            },
            {
                withhold["stance"],
                conditional["stance"],
                volunteer["stance"],
                accept["stance"],
                refuse["stance"],
            },
        )

    def test_model_cannot_mutate_rules_or_choose_a_different_stance(self) -> None:
        simulator = started("control_room")
        before = simulator.deterministic_outcome()
        stance = simulator.resolve_npc_stance("gu_heng", "status_update")[
            "stance"
        ]
        expression = simulator.render_npc_expression(
            "gu_heng",
            "status_update",
            provider_response={
                "stance": "accept" if stance != "accept" else "refuse",
                "utterance": "我直接把发电机修好了。",
                "task_progress": 2,
                "referenced_fact_ids": [],
            },
            online=True,
        )
        self.assertEqual(stance, expression["stance"])
        self.assertEqual(before, simulator.deterministic_outcome())
        self.assertEqual(
            "model_attempted_rule_change",
            simulator.state["expression_log"][-1]["validation_reason"],
        )

    def test_npc_context_does_not_leak_other_npc_secret(self) -> None:
        simulator = started("control_room")
        gu_context = simulator.build_agent_context("gu_heng", "status_update")
        ye_context = simulator.build_agent_context("ye_cheng", "status_update")
        self.assertNotIn("FACT_HEAT_PACK", gu_context["allowed_fact_ids"])
        self.assertNotIn(
            "FACT_RELAY_COMPATIBILITY", ye_context["allowed_fact_ids"]
        )


class RouteAndEndingTests(unittest.TestCase):
    def test_three_success_routes_have_distinct_paid_ap_and_costs(self) -> None:
        expected = {
            "medical_cooperation": (10, "stable_rescue"),
            "technical_savings": (9, "stable_rescue"),
            "risk_push": (8, "signal_sent_cost_uncontrolled"),
        }
        outputs: dict[str, dict] = {}
        for route_id, (paid_ap, ending) in expected.items():
            simulator = WhiteoutSimulatorV11()
            output = run_route(simulator, route_id)
            outputs[route_id] = output
            self.assertTrue(simulator.state["tasks"]["signal_sent"])
            self.assertEqual(paid_ap, output["paid_ap"])
            self.assertEqual(ending, output["ending"])

        self.assertEqual(
            0,
            outputs["medical_cooperation"]["score"]["breakdown"][
                "information_responsibility"
            ]
            % 0.5,
        )
        medical = WhiteoutSimulatorV11()
        run_route(medical, "medical_cooperation")
        technical = WhiteoutSimulatorV11()
        run_route(technical, "technical_savings")
        risk = WhiteoutSimulatorV11()
        run_route(risk, "risk_push")
        self.assertEqual(0, medical.state["resources"]["medicine"])
        self.assertFalse(technical.state["flags"]["kitchen_heater_intact"])
        self.assertEqual("critical", risk._injury_level("gu_heng"))

    def test_two_failure_routes_reach_distinct_failure_endings(self) -> None:
        expected = {
            "warm_wait_failure": "warm_wait_unknown",
            "dual_collapse_failure": "dual_collapse",
        }
        for route_id, ending in expected.items():
            simulator = WhiteoutSimulatorV11()
            output = run_route(simulator, route_id)
            self.assertFalse(simulator.state["tasks"]["signal_sent"])
            self.assertEqual(ending, output["ending"])
            self.assertFalse(output["expected_success"])

    def test_all_four_endings_are_reachable_through_routes(self) -> None:
        route_ids = (
            "medical_cooperation",
            "risk_push",
            "warm_wait_failure",
            "dual_collapse_failure",
        )
        endings = {
            run_route(WhiteoutSimulatorV11(), route_id)["ending"]
            for route_id in route_ids
        }
        self.assertEqual(
            {
                "stable_rescue",
                "signal_sent_cost_uncontrolled",
                "warm_wait_unknown",
                "dual_collapse",
            },
            endings,
        )

    def test_offline_and_online_expression_paths_have_identical_rule_outcomes(self) -> None:
        for route_id in (
            "medical_cooperation",
            "technical_savings",
            "risk_push",
        ):
            with self.subTest(route=route_id):
                offline = WhiteoutSimulatorV11()
                online = WhiteoutSimulatorV11()
                run_route(offline, route_id, online=False)
                run_route(online, route_id, online=True)
                self.assertEqual(
                    offline.deterministic_outcome(),
                    online.deterministic_outcome(),
                )

    def test_causal_timeline_contains_assertable_choice_result_and_followup(self) -> None:
        output = run_route(WhiteoutSimulatorV11(), "technical_savings")
        self.assertGreaterEqual(len(output["timeline"]), 4)
        self.assertLessEqual(len(output["timeline"]), 6)
        for item in output["timeline"]:
            self.assertEqual(
                {"phase", "choice", "immediate", "follow_up"}, set(item)
            )


class ScoreTests(unittest.TestCase):
    def test_no_signal_caps_rating_at_c_and_critical_person_caps_at_b(self) -> None:
        no_signal = WhiteoutSimulatorV11()
        no_signal_score = no_signal.calculate_score()
        self.assertEqual("C", no_signal_score["rating_cap"])
        self.assertGreaterEqual(
            RATING_ORDER.index(no_signal_score["rating"]),
            RATING_ORDER.index("C"),
        )

        critical = WhiteoutSimulatorV11()
        critical.state["tasks"]["signal_sent"] = True
        critical.state["characters"]["gu_heng"]["injuries"] = [
            "right_hand_critical"
        ]
        critical_score = critical.calculate_score()
        self.assertEqual("B", critical_score["rating_cap"])
        self.assertGreaterEqual(
            RATING_ORDER.index(critical_score["rating"]),
            RATING_ORDER.index("B"),
        )

    def test_hoarded_food_medicine_and_fuel_are_discounted_during_emergency(self) -> None:
        food = WhiteoutSimulatorV11()
        food.state["characters"]["player"]["stamina"] = 0
        food_with = food.calculate_score()["breakdown"]["effective_reserves"]
        food.state["resources"]["food"] = 0
        food_without = food.calculate_score()["breakdown"]["effective_reserves"]
        self.assertLessEqual(food_with - food_without, 1.0)

        medicine = WhiteoutSimulatorV11()
        medicine.state["characters"]["gu_heng"]["injuries"] = [
            "right_hand_critical"
        ]
        medicine.state["resources"]["heat_pack"] = 0
        medicine_with = medicine.calculate_score()["breakdown"][
            "effective_reserves"
        ]
        medicine.state["resources"]["medicine"] = 0
        medicine_without = medicine.calculate_score()["breakdown"][
            "effective_reserves"
        ]
        self.assertLessEqual(medicine_with - medicine_without, 1.0)

        fuel = WhiteoutSimulatorV11()
        fuel.state["characters"]["player"]["temperature"] = 3.0
        fuel_with = fuel.calculate_score()["breakdown"]["effective_reserves"]
        fuel.state["resources"]["fuel"] = 0
        fuel_without = fuel.calculate_score()["breakdown"]["effective_reserves"]
        self.assertLessEqual(fuel_with - fuel_without, 1.25)

    def test_score_is_clamped_to_zero_through_one_hundred(self) -> None:
        for route_id in load_rules()["routes"]:
            score = run_route(WhiteoutSimulatorV11(), route_id)["score"]["total"]
            self.assertGreaterEqual(score, 0)
            self.assertLessEqual(score, 100)


if __name__ == "__main__":
    unittest.main()
