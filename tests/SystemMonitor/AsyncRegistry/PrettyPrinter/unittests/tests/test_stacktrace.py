import unittest

from asyncregistry.stacktrace import Stacktrace

class StacktraceTest(unittest.TestCase):
    def test_directly_gives_back_hierarchy_zero_entry(self):
        stacktrace = Stacktrace()
        self.assertEqual(stacktrace.append(0, 'entry'), [('  ┌ ', 'entry')])

    def test_indents_consecutively_descending_hierarchie_levels(self):
        stacktrace = Stacktrace()
        self.assertEqual(stacktrace.append(3, 'hierarchy 3'), None)
        self.assertEqual(stacktrace.append(2, 'hierarchy 2'), None)
        self.assertEqual(stacktrace.append(1, 'hierarchy 1'), None)
        self.assertEqual(stacktrace.append(0, 'hierarchy 0'), 
                         [
                             ('        ┌ ', 'hierarchy 3'),
                             ('      ┌ ', 'hierarchy 2'),
                             ('    ┌ ', 'hierarchy 1'),
                             ('  ┌ ', 'hierarchy 0')
                         ])

    def test_combines_lines_of_consecutive_entries_of_the_same_hierarchy_level(self):
        stacktrace = Stacktrace()
        self.assertEqual(stacktrace.append(1, 'hierarchy 1 a'), None)
        self.assertEqual(stacktrace.append(1, 'hierarchy 1 b'), None)
        self.assertEqual(stacktrace.append(0, 'hierarchy 0'),
                         [
                             ('    ┌ ', 'hierarchy 1 a'),
                             ('    ├ ', 'hierarchy 1 b'),
                             ('  ┌ ', 'hierarchy 0')
                         ])

    def test_continues_lines_of_non_finished_hierarchies(self):
        stacktrace = Stacktrace()
        self.assertEqual(stacktrace.append(1, 'hierarchy 1 a'), None)
        self.assertEqual(stacktrace.append(2, 'hierarchy 2 a'), None)
        self.assertEqual(stacktrace.append(3, 'hierarchy 3'), None)
        self.assertEqual(stacktrace.append(2, 'hierarchy 2 b'), None)
        self.assertEqual(stacktrace.append(1, 'hierarchy 1 b'), None)
        self.assertEqual(stacktrace.append(0, 'hierarchy 0'),
                         [
                             ('    ┌ ', 'hierarchy 1 a'),
                             ('    │ ┌ ', 'hierarchy 2 a'),
                             ('    │ │ ┌ ', 'hierarchy 3'),
                             ('    │ ├ ', 'hierarchy 2 b'),
                             ('    ├ ', 'hierarchy 1 b'),
                             ('  ┌ ', 'hierarchy 0')
                         ])

    def test_works_also_for_non_consecutive_hierarchy_levels(self):
        stacktrace = Stacktrace()
        self.assertEqual(stacktrace.append(1, 'hierarchy 1'), None)
        self.assertEqual(stacktrace.append(5, 'hierarchy 5'), None)
        self.assertEqual(stacktrace.append(2, 'hierarchy 2'), None)
        self.assertEqual(stacktrace.append(0, 'hierarchy 0'),
                         [
                             ('    ┌ ', 'hierarchy 1'),
                             ('    │       ┌ ', 'hierarchy 5'),
                             ('    │ ┌ ', 'hierarchy 2'),
                             ('  ┌ ', 'hierarchy 0')
                         ])


            

if __name__ == '__main__':
    unittest.main()
