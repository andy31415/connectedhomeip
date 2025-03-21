# Copyright (c) 2022 Project CHIP Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import os

from matter_idl.generators import CodeGenerator, GeneratorStorage
from matter_idl.generators.cluster_selection import server_side_clusters
from matter_idl.matter_idl_types import Idl, ServerClusterInstantiation

from dataclasses import dataclass

@dataclass
class ServerClusterConfig:
    endpoint_number: int
    cluster_name: str
    feature_map: int
    cluster_revision: int
    instance: ServerClusterInstantiation



class CppApplicationGenerator(CodeGenerator):
    """
    Generation of cpp code for application implementation for matter.
    """

    def __init__(self, storage: GeneratorStorage, idl: Idl, **kargs):
        """
        Inintialization is specific for java generation and will add
        filters as required by the java .jinja templates to function.
        """
        super().__init__(storage, idl, fs_loader_searchpath=os.path.dirname(__file__))

    def internal_render_all(self):
        """
        Renders the cpp and header files required for applications
        """

        # Header containing a macro to initialize all cluster plugins
        self.internal_render_one_output(
            template_path="PluginApplicationCallbacksHeader.jinja",
            output_file_name="app/PluginApplicationCallbacks.h",
            vars={
                'clusters': server_side_clusters(self.idl)
            }
        )

        # Source for __attribute__(weak) implementations of all cluster
        # initialization methods
        self.internal_render_one_output(
            template_path="CallbackStubSource.jinja",
            output_file_name="app/callback-stub.cpp",
            vars={
                'clusters': server_side_clusters(self.idl)
            }
        )

        self.internal_render_one_output(
            template_path="ClusterInitCallbackSource.jinja",
            output_file_name="app/cluster-init-callback.cpp",
            vars={
                'clusters': server_side_clusters(self.idl)
            }
        )

        # Map of cluster names to actual cluster data
        endpoint_infos = {}

        # Generating metadata for every cluster
        for endpoint in self.idl.endpoints:
            for server_cluster in endpoint.server_clusters:

                # Defaults as per spec, however ZAP should generally
                # contain valid values here as they are required
                feature_map = 0
                cluster_revision = 1

                for attribute in server_cluster.attributes:
                    if attribute.default is None:
                        continue

                    match attribute.name:
                        case 'featureMap':
                            assert isinstance(attribute.default, int)
                            feature_map = attribute.default
                        case 'clusterRevision':
                            assert isinstance(attribute.default, int)
                            cluster_revision = attribute.default
                        case _:
                            # no other attributes are interesting at this point
                            # although we may want to pull in some defaults
                            pass

                name = server_cluster.name
                if name not in endpoint_infos:
                    endpoint_infos[name] = []

                endpoint_infos[name].append(
                    ServerClusterConfig(
                        endpoint_number=endpoint.number,
                        cluster_name=name,
                        feature_map=feature_map,
                        cluster_revision = cluster_revision,
                        instance = server_cluster,
                    )
                )

        for name, instances in endpoint_infos.items():
            self.internal_render_one_output(
                template_path="ServerClusterConfig.jinja",
                output_file_name=f"app/cluster-config/{name}.h",
                vars={
                    'cluster_name': name,
                    'instances': instances
                }
            )

