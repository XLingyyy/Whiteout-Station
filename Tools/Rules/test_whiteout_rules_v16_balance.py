"""Focused rebalance regressions; UE automation remains the authority."""
import copy
import json
from pathlib import Path
import unittest

from whiteout_rules_v11 import WhiteoutSimulatorV11


class BalanceV16(unittest.TestCase):
    def setUp(self):
        path = Path(__file__).resolve().parents[2] / 'WhiteoutStation/Content/Rules/WhiteoutStationRules.v1.6.json'
        self.sim = WhiteoutSimulatorV11(json.loads(path.read_text(encoding='utf-8-sig')))
        self.sim.start_phase('medical_room')

    def test_rest_every_person_and_cold_wait(self):
        for who in ('player', 'gu_heng', 'ye_cheng'):
            with self.subTest(who=who):
                self.setUp()
                person = self.sim.state['characters'][who]
                person.update(temperature=5.2, stamina=2)
                before_pressure = person['pressure']
                result = self.sim.apply_action('rest', dict(target=who, location='medical_room'))
                self.assertTrue(result.committed)
                self.assertAlmostEqual(person['temperature'], 6.2)
                self.assertAlmostEqual(person['pressure'], before_pressure - .4)
                self.assertEqual(person['stamina'], 2)
                self.sim.apply_action('rest', dict(target=who, location='kitchen'))
                self.assertAlmostEqual(person['temperature'], 6.2)
                self.assertAlmostEqual(person['pressure'], before_pressure - .6)

    def test_split_food_no_exclusion_or_repeat_trust_reward(self):
        gu, ye = (self.sim.state['characters'][who] for who in ('gu_heng', 'ye_cheng'))
        self.sim.state['characters']['player']['stamina'] = 0
        first = self.sim.apply_action('distribute_food', dict(recipients=['gu_heng']))
        self.assertTrue(first.committed)
        self.assertEqual(first.ap_before - first.ap_after, 1)
        self.assertEqual(gu['trust'], 4)
        self.assertEqual(ye['trust'], 6)
        self.sim.apply_action('distribute_food', dict(recipients=['gu_heng']))
        self.assertEqual(gu['trust'], 4)
        before = copy.deepcopy(self.sim.state)
        result = self.sim.apply_action('distribute_food', dict(recipients=['player', 'ye_cheng']))
        self.assertFalse(result.committed)
        self.assertEqual(before, self.sim.state)

    def test_three_person_hot_meal(self):
        self.sim.new_game()
        self.sim.start_phase('kitchen')
        before = copy.deepcopy(self.sim.state['characters'])
        result = self.sim.apply_action('distribute_food', dict(recipients=list(before), meal_type='hot'))
        self.assertTrue(result.committed)
        for who, person in self.sim.state['characters'].items():
            self.assertAlmostEqual(person['temperature'], before[who]['temperature'] + .5)
            self.assertEqual(person['stamina'], 2)
        self.assertEqual(self.sim.state['resources']['food'], 0)

    def test_collaboration_willingness_thresholds(self):
        gu = self.sim.state['characters']['gu_heng']
        for trust, pressure, available, ap in [(3,7,True,2),(4.5,7,True,1),(6,8,True,2),(2.9,7,False,1),(6,9,False,1)]:
            gu.update(trust=trust, pressure=pressure)
            quote = self.sim.get_action_cost('inspect_control_cabinet', dict(collaborator='gu_heng'))
            self.assertEqual(quote['can_execute'], available)
            if available:
                self.assertEqual(quote['final_ap'], ap)

    def test_preparation_committed_once(self):
        result = self.sim.apply_action('inspect_control_cabinet', dict(collaborator='gu_heng'), transaction_id='inspection')
        self.assertTrue(result.committed)
        self.assertTrue(self.sim.state['repair_preparation_available'])
        self.assertFalse(self.sim.apply_action('inspect_control_cabinet', dict(collaborator='gu_heng'), transaction_id='inspection').committed)
        self.assertFalse(self.sim.apply_action('repair_generator').committed)
        self.assertTrue(self.sim.state['repair_preparation_available'])
        self.sim.settle_phase()
        self.sim.start_phase('repair_room')
        gu = self.sim.state['characters']['gu_heng']
        gu.update(stamina=2, trust=6, injuries=[])
        quote = self.sim.get_action_cost('repair_generator')
        self.assertTrue(quote['stamina_waived'])
        self.assertTrue(self.sim.apply_action('repair_generator').committed)
        self.assertEqual(gu['stamina'], 2)
        self.assertFalse(self.sim.state['repair_preparation_available'])


if __name__ == '__main__':
    unittest.main()
