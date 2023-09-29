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

import enum
import logging
from dataclasses import dataclass
from typing import Dict, Iterable, Optional, Tuple

from yaml import safe_load


class MemberType(enum.Enum):
    """Supported member types for unambiguous key definitions."""
    ATTRIBUTE = enum.auto()
    BITMAP = enum.auto()
    COMMAND = enum.auto()
    ENUM = enum.auto()
    EVENT = enum.auto()
    STRUCT = enum.auto()

    @classmethod
    def extract_type(cls, value: str) -> Tuple[str, 'MemberType']:
        """Split out a string of the form <name/type> into actual parts. """
        parts = value.split('/')
        if len(parts) == 1:
            return value, None
        assert (len(parts) == 2)

        # somewhat lenient here: just uppercase everything
        return parts[0], MemberType[parts[1].upper()]


@dataclass(eq=True, frozen=True)
class Key:
    """Represents a unique key within a version information data section."""

    # cluster name. None ONLY for global attributes
    cluster: Optional[str] = None
    # name of member within the cluster
    member: Optional[str] = None
    member_type: Optional[MemberType] = None  # type if non-fuzzy member name
    field: Optional[str] = None               # Field name

    def __post_init__(self):
        """Validates correct formatting of the key."""
        if self.cluster is None:
            # This is "*.<name>" representing global attributes
            assert (self.field is None)
            assert (self.member_type is None)
            return

        if self.field is not None:
            # fields can only be defined on members
            assert (self.member is not None)

        if self.member_type is not None:
            # if we have a member type, we should have a member name
            assert (self.member is not None)

    @classmethod
    def from_string(cls, s: str) -> 'Key':
        parts = s.split('.')
        if len(parts) == 1:
            # just cluster
            return Key(cluster=s)
        elif len(parts) == 2:
            if parts[0] == '*':
                return Key(member=parts[1])  # global attribute

        assert (len(parts) > 0)
        assert (len(parts) <= 3)

        cluster = parts[0]
        member, member_type = MemberType.extract_type(parts[1])
        field = parts[2] if len(parts) > 2 else None

        return Key(cluster=cluster, member=member, member_type=member_type, field=field)


@dataclass(frozen=True)
class MetaData:
    # "new", "hidden", "provisional", "alias=Xyz", ...
    value: str
    versions: Dict[str, str]  # actual versions applied to this value

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
        self.data: Dict[Key, List[MetaData]] = {}

    def load_file(self, path: str, only_tags: Optional[Iterable[str]] = None):
        """Load the given YAML file.

        Args:
           path - the file to load
           only_keys - if set, only load the tags with the given PREFIXES
        """
        self.logger.info("Loading %s", path)

        with open(path, 'r') as stream:
            data = safe_load(stream)

            versions = data['versions']

            for k, values in data['data'].items():
                key = Key.from_string(k)
                if key in self.data:
                    metadata = self.data[key]
                else:
                    metadata = []

                if only_tags:
                    # filter out values
                    values = [v for v in values
                              if any([v.startswith(prefix) for prefix in only_tags])]
                    if not values:
                        continue

                metadata.extend(
                    [MetaData(versions=versions, value=value) for value in values])
                self.logger.info("Loaded more data for %r", key)
                self.data[key] = metadata

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

        # First file load everything, subsequent only new
        only_tags = None
        for name in files:
            info.load_file(name, only_tags)
            only_tags = ['new']

        # TODO: print out or do something useful here,
        #       like some form of query on info

    main(auto_envvar_prefix='CHIP')
