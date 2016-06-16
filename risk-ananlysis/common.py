# -*- coding:utf-8 -*-

from fault_tree_lib import Primary as P


def software_bug():
    return P('software failure (bug)\n(e.g., unhandled edge case)',
             failure_rate=1e-2)
