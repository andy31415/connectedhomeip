#!/usr/bin/env python
#
#    Copyright (c) 2023 Project CHIP Authors
#
#    Licensed under the Apache License, Version 2.0 (the "License");
#    you may not use this file except in compliance with the License.
#    You may obtain a copy of the License at
#
#        http://www.apache.org/licenses/LICENSE-2.0
#
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS,
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#    See the License for the specific language governing permissions and
#    limitations under the License.
#

import logging
import enum
from yaml import safe_load
from dataclasses import dataclass
from typing import Optional


class MemberType(enum.Enum):
    """Supported member types for unambiguous key definitions."""
    ATTRIBUTE = enum.auto()
    BITMAP = enum.auto()
    COMMAND = enum.auto()
    ENUM = enum.auto()
    EVENT = enum.auto()
    STRUCT = enum.auto()


@dataclass(eq=True, frozen=True)
class Key:
    """Represents a unique key within a version information data section."""

    cluster: Optional[str]                    # cluster name. None ONLY for global attributes
    member: Optional[str] = None              # name of member within the cluster
    member_type: Optional[MemberType] = None  # type if non-fuzzy member name
    field: Optional[str] = None               # Field name

    def __post_init__(self):
        """Validates correct formatting of the key."""
        if self.cluster is None:
            # This is "*.<name>" representing global attributes
            assert(self.field is None)
            assert(self.member_type is None)
            return

        if self.field is not None:
            # fields can only be defined on members
            assert(self.member is not None)

        if self.member_type is not None:
            # if we have a member type, we should have a member name
            assert(self.member is not None)

    @classmethod
    def from_string(cls, s: str) -> 'Key':
        # FIXME: implement
        return Key(cluster=s)


# TODO:
#   - alias-info: has aliases and versions when they got added

# Loading logic:
#   - load everything required if needed
#   - new could be inherited (if none of provisional/hidden exist)

class VersionInformation:
    """
    Handles loading and management of version files.

    In particular a version file contains:
      - 'versions' - map of name to human readable version (like "kotlin": "2.3.5")
      - 'data' - maps "item keys" (clusters/cluster-members/cluster-member-fields) to
        a list of metadata including new/provisional/hidden
    """

    def __init__(self):
        self.logger = logging.getLogger(__name__)

    def load_file(self, path: str):
        self.logger.info("Loading %s", path)

        with open(path, 'r') as stream:
           data = safe_load(stream)

           for k, v in data['data'].items():
               self.logger.info("Loaded: %r -> %r", Key.from_string(k), v)

        # FIXME: process the data ...

        self.logger.info("Done loading %s", path)

if __name__ == '__main__':
    import click

    # Supported log levels, mapping string values required for argument
    # parsing into logging constants
    __LOG_LEVELS__ = {
        'debug': logging.DEBUG,
        'info': logging.INFO,
        'warn': logging.WARN,
        'fatal': logging.FATAL,
    }

    @click.command()
    @click.option(
        '--log-level',
        default='INFO',
        type=click.Choice(list(__LOG_LEVELS__.keys()), case_sensitive=False),
        help='Determines the verbosity of script output.')
    @click.argument('files', nargs=-1, type=click.Path())
    def main(log_level, files):
        logging.basicConfig(
            level=__LOG_LEVELS__[log_level],
            format='%(asctime)s %(levelname)-7s %(message)s',
        )

        info = VersionInformation()

        for name in files:
            info.load_file(name)

        # TODO: print out or do something useful here,
        #       like some form of query on info

    main(auto_envvar_prefix='CHIP')

