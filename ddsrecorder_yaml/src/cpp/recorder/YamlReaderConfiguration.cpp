// Copyright 2023 Proyectos y Sistemas de Mantenimiento SL (eProsima).
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/**
 * @file YamlReaderConfiguration.cpp
 *
 */

#include <cpp_utils/utils.hpp>

#include <ddspipe_core/types/dynamic_types/types.hpp>
#include <ddspipe_core/types/topic/filter/ManualTopic.hpp>
#include <ddspipe_core/types/topic/filter/WildcardDdsFilterTopic.hpp>
#include <ddspipe_participants/types/address/Address.hpp>

#include <ddspipe_yaml/yaml_configuration_tags.hpp>
#include <ddspipe_yaml/Yaml.hpp>
#include <ddspipe_yaml/YamlManager.hpp>

#include <ddsrecorder_yaml/recorder/yaml_configuration_tags.hpp>

#include <ddsrecorder_yaml/recorder/YamlReaderConfiguration.hpp>

namespace eprosima {
namespace ddsrecorder {
namespace yaml {

using namespace eprosima::ddspipe::core;
using namespace eprosima::ddspipe::core::types;
using namespace eprosima::ddspipe::participants;
using namespace eprosima::ddspipe::participants::types;
using namespace eprosima::ddspipe::yaml;

RecorderConfiguration::RecorderConfiguration(
        const Yaml& yml)
{
    load_ddsrecorder_configuration_(yml);
}

RecorderConfiguration::RecorderConfiguration(
        const std::string& file_path)
{
    load_ddsrecorder_configuration_from_file_(file_path);
}

void RecorderConfiguration::load_ddsrecorder_configuration_(
        const Yaml& yml)
{
    try
    {
        YamlReaderVersion version = LATEST;

        ////////////////////////////////////////
        // Create participants configurations //
        ////////////////////////////////////////

        /////
        // Create Simple Participant Configuration
        simple_configuration = std::make_shared<SimpleParticipantConfiguration>();
        simple_configuration->id = "SimpleRecorderParticipant";
        simple_configuration->app_id = "DDS_RECORDER";
        simple_configuration->app_metadata = "";
        simple_configuration->is_repeater = false;

        /////
        // Create Recorder Participant Configuration
        recorder_configuration = std::make_shared<ParticipantConfiguration>();
        recorder_configuration->id = "RecorderRecorderParticipant";
        recorder_configuration->app_id = "DDS_RECORDER";
        // TODO: fill metadata field once its content has been defined.
        recorder_configuration->app_metadata = "";
        recorder_configuration->is_repeater = false;

        /////
        // Get optional Recorder configuration options
        if (YamlReader::is_tag_present(yml, RECORDER_RECORDER_TAG))
        {
            auto recorder_yml = YamlReader::get_value_in_tag(yml, RECORDER_RECORDER_TAG);
            load_recorder_configuration_(recorder_yml, version);
        }

        // Initialize cleanup_period with twice the value of event_window
        // WARNING: event_window tag (under recorder tag) must have been parsed beforehand
        cleanup_period = 2 * event_window;

        /////
        // Get optional specs configuration
        // WARNING: Parse builtin topics (dds tag) AFTER specs, as some topic-specific default values are set there
        if (YamlReader::is_tag_present(yml, SPECS_TAG))
        {
            auto specs_yml = YamlReader::get_value_in_tag(yml, SPECS_TAG);
            load_specs_configuration_(specs_yml, version);
        }

        /////
        // Get optional DDS configuration options
        if (YamlReader::is_tag_present(yml, RECORDER_DDS_TAG))
        {
            auto dds_yml = YamlReader::get_value_in_tag(yml, RECORDER_DDS_TAG);
            load_dds_configuration_(dds_yml, version);
        }

        // Block ROS 2 services (RPC) topics
        // RATIONALE:
        // At the time of this writting, services in ROS 2 behave in the following manner: a ROS 2 service
        // client awaits to discover a server, and it is then when a request is sent to this (and only this) server,
        // from which a response is expected.
        // Hence, if these topics are not blocked, the client would wrongly believe DDS-Recorder is a server, thus
        // sending a request for which a response will not be received.
        WildcardDdsFilterTopic rpc_request_topic, rpc_response_topic;
        rpc_request_topic.topic_name.set_value("rq/*");
        rpc_response_topic.topic_name.set_value("rr/*");

        ddspipe_configuration.blocklist.insert(
            utils::Heritable<WildcardDdsFilterTopic>::make_heritable(rpc_request_topic));

        ddspipe_configuration.blocklist.insert(
            utils::Heritable<WildcardDdsFilterTopic>::make_heritable(rpc_response_topic));

        ddspipe_configuration.init_enabled = true;

        // Only trigger the DdsPipe's callbacks when discovering or removing writers
        ddspipe_configuration.discovery_trigger = DiscoveryTrigger::WRITER;

        // Initialize controller domain with the same as the one being recorded
        // WARNING: dds tag must have been parsed beforehand
        controller_domain = simple_configuration->domain;

        /////
        // Get optional remote controller configuration
        if (YamlReader::is_tag_present(yml, RECORDER_REMOTE_CONTROLLER_TAG))
        {
            auto controller_yml = YamlReader::get_value_in_tag(yml, RECORDER_REMOTE_CONTROLLER_TAG);
            load_controller_configuration_(controller_yml, version);
        }

        // Get optional identify ca file path
        if (YamlReader::is_tag_present(yml, IDENTIFY_CA_FILE_PATH_TAG))
        {
            simple_configuration->use_security = true;
            simple_configuration->identify_ca_fname = "file://" + YamlReader::get<std::string>(yml, IDENTIFY_CA_FILE_PATH_TAG, version);
            std::cout << "Identify ca file path: " << simple_configuration->identify_ca_fname << std::endl;
        }
        // Get optional identify cert file path
        if (YamlReader::is_tag_present(yml, IDENTIFY_CERT_FILE_PATH_TAG))
        {
            simple_configuration->identity_cert_fname = "file://" + YamlReader::get<std::string>(yml, IDENTIFY_CERT_FILE_PATH_TAG, version);
            std::cout << "Identify cert file path: " << simple_configuration->identity_cert_fname << std::endl;
        }
        // Get optional identify key file path
        if (YamlReader::is_tag_present(yml, IDENTIFY_KEY_FILE_PATH_TAG))
        {
            simple_configuration->identity_key_fname = "file://" + YamlReader::get<std::string>(yml, IDENTIFY_KEY_FILE_PATH_TAG, version);
            std::cout << "Key file path: " << simple_configuration->identity_key_fname << std::endl;
        }
        // Get optional governance file path
        if (YamlReader::is_tag_present(yml, GOVERNANCE_FILE_PATH_TAG))
        {
            simple_configuration->governance_fname = "file://" + YamlReader::get<std::string>(yml, GOVERNANCE_FILE_PATH_TAG, version);
            std::cout << "Governance file path: " << simple_configuration->governance_fname << std::endl;
        }
        // Get optional permission file path
        if (YamlReader::is_tag_present(yml, PERMISSION_FILE_PATH_TAG))
        {
            simple_configuration->permission_fname = "file://" + YamlReader::get<std::string>(yml, PERMISSION_FILE_PATH_TAG, version);
            std::cout << "Permission file path: " << simple_configuration->permission_fname << std::endl;
        }

        // Get optional influxdb configuration
        if (YamlReader::is_tag_present(yml, INFLUXDB_SEVER_URL))
        {
            enable_influxdb = true;
            influxdb_server_url = YamlReader::get<std::string>(yml, INFLUXDB_SEVER_URL, version);
            std::cout << "Influxdb server url: " << influxdb_server_url << std::endl;
        }
        // Get optional influxdb bucket
        if (YamlReader::is_tag_present(yml, INFLUXDB_BUCKET))
        {
            influxdb_bucket = YamlReader::get<std::string>(yml, INFLUXDB_BUCKET, version);
            std::cout << "Influxdb bucket: " << influxdb_bucket << std::endl;
        }
        // Get optional influxdb organization
        if (YamlReader::is_tag_present(yml, INFLUXDB_ORGANIZATION))
        {
            influxdb_organization = YamlReader::get<std::string>(yml, INFLUXDB_ORGANIZATION, version);
            std::cout << "Influxdb organization: " << influxdb_organization << std::endl;
        }
        // Get optional influxdb token
        if (YamlReader::is_tag_present(yml, INFLUXDB_TOKEN))
        {
            influxdb_token = YamlReader::get<std::string>(yml, INFLUXDB_TOKEN, version);
            std::cout << "Influxdb token: " << influxdb_token << std::endl;
        }
    }
    catch (const std::exception& e)
    {
        throw eprosima::utils::ConfigurationException(
                  utils::Formatter() << "Error loading DDS Recorder configuration from yaml:\n " << e.what());
    }
}

void RecorderConfiguration::load_recorder_configuration_(
        const Yaml& yml,
        const YamlReaderVersion& version)
{
    if (YamlReader::is_tag_present(yml, RECORDER_OUTPUT_TAG))
    {
        auto output_yml = YamlReader::get_value_in_tag(yml, RECORDER_OUTPUT_TAG);

        /////
        // Get optional file path
        if (YamlReader::is_tag_present(output_yml, RECORDER_OUTPUT_PATH_FILE_TAG))
        {
            output_filepath = YamlReader::get<std::string>(output_yml, RECORDER_OUTPUT_PATH_FILE_TAG, version);
        }

        /////
        // Get optional file name
        if (YamlReader::is_tag_present(output_yml, RECORDER_OUTPUT_FILE_NAME_TAG))
        {
            output_filename = YamlReader::get<std::string>(output_yml, RECORDER_OUTPUT_FILE_NAME_TAG, version);
        }

        /////
        // Get optional timestamp format
        if (YamlReader::is_tag_present(output_yml, RECORDER_OUTPUT_TIMESTAMP_FORMAT_TAG))
        {
            output_timestamp_format = YamlReader::get<std::string>(output_yml, RECORDER_OUTPUT_TIMESTAMP_FORMAT_TAG,
                            version);
        }

        /////
        // Get optional timestamp format
        if (YamlReader::is_tag_present(output_yml, RECORDER_OUTPUT_LOCAL_TIMESTAMP_TAG))
        {
            output_local_timestamp = YamlReader::get<bool>(output_yml, RECORDER_OUTPUT_LOCAL_TIMESTAMP_TAG, version);
        }
    }

    /////
    // Get optional buffer size
    if (YamlReader::is_tag_present(yml, RECORDER_BUFFER_SIZE_TAG))
    {
        buffer_size = YamlReader::get_positive_int(yml, RECORDER_BUFFER_SIZE_TAG);
    }

    /////
    // Get optional event window length
    if (YamlReader::is_tag_present(yml, RECORDER_EVENT_WINDOW_TAG))
    {
        event_window = YamlReader::get_positive_int(yml, RECORDER_EVENT_WINDOW_TAG);
    }

    /////
    // Get optional log publishTime
    if (YamlReader::is_tag_present(yml, RECORDER_LOG_PUBLISH_TIME_TAG))
    {
        log_publish_time = YamlReader::get<bool>(yml, RECORDER_LOG_PUBLISH_TIME_TAG, version);
    }

    /////
    // Get optional only_with_type
    if (YamlReader::is_tag_present(yml, RECORDER_ONLY_WITH_TYPE_TAG))
    {
        only_with_type = YamlReader::get<bool>(yml, RECORDER_ONLY_WITH_TYPE_TAG, version);
    }

    /////
    // Get optional compression settings
    if (YamlReader::is_tag_present(yml, RECORDER_COMPRESSION_SETTINGS_TAG))
    {
        mcap_writer_options = YamlReader::get<mcap::McapWriterOptions>(yml, RECORDER_COMPRESSION_SETTINGS_TAG, version);
    }

    /////
    // Get optional record_types
    if (YamlReader::is_tag_present(yml, RECORDER_RECORD_TYPES_TAG))
    {
        record_types = YamlReader::get<bool>(yml, RECORDER_RECORD_TYPES_TAG, version);
    }

    /////
    // Get optional ros2_types
    if (YamlReader::is_tag_present(yml, RECORDER_ROS2_TYPES_TAG))
    {
        ros2_types = YamlReader::get<bool>(yml, RECORDER_ROS2_TYPES_TAG, version);
    }
}

void RecorderConfiguration::load_controller_configuration_(
        const Yaml& yml,
        const YamlReaderVersion& version)
{
    // Get optional enable remote controller
    if (YamlReader::is_tag_present(yml, RECORDER_REMOTE_CONTROLLER_ENABLE_TAG))
    {
        enable_remote_controller = YamlReader::get<bool>(yml, RECORDER_REMOTE_CONTROLLER_ENABLE_TAG,
                        version);
    }

    // Get optional DDS domain
    if (YamlReader::is_tag_present(yml, DOMAIN_ID_TAG))
    {
        controller_domain = YamlReader::get<DomainId>(yml, DOMAIN_ID_TAG, version);
    }

    // Get optional initial state
    if (YamlReader::is_tag_present(yml, RECORDER_REMOTE_CONTROLLER_INITIAL_STATE_TAG))
    {
        // Convert to enum and check valid wherever used to avoid mcap library dependency in YAML module
        initial_state = YamlReader::get<std::string>(yml,
                        RECORDER_REMOTE_CONTROLLER_INITIAL_STATE_TAG, version);
        // Case insensitive
        eprosima::utils::to_uppercase(initial_state);
    }

    // Get optional command topic name
    if (YamlReader::is_tag_present(yml, RECORDER_REMOTE_CONTROLLER_COMMAND_TOPIC_NAME_TAG))
    {
        command_topic_name = YamlReader::get<std::string>(yml,
                        RECORDER_REMOTE_CONTROLLER_COMMAND_TOPIC_NAME_TAG, version);
    }

    // Get optional status topic name
    if (YamlReader::is_tag_present(yml, RECORDER_REMOTE_CONTROLLER_STATUS_TOPIC_NAME_TAG))
    {
        status_topic_name = YamlReader::get<std::string>(yml,
                        RECORDER_REMOTE_CONTROLLER_STATUS_TOPIC_NAME_TAG, version);
    }
}

void RecorderConfiguration::load_specs_configuration_(
        const Yaml& yml,
        const YamlReaderVersion& version)
{
    // Get number of threads
    if (YamlReader::is_tag_present(yml, NUMBER_THREADS_TAG))
    {
        n_threads = YamlReader::get_positive_int(yml, NUMBER_THREADS_TAG);
    }

    /////
    // Get optional Topic QoS
    if (YamlReader::is_tag_present(yml, SPECS_QOS_TAG))
    {
        YamlReader::fill<TopicQoS>(topic_qos, YamlReader::get_value_in_tag(yml, SPECS_QOS_TAG), version);
        TopicQoS::default_topic_qos.set_value(topic_qos);
    }

    // Get max pending samples
    if (YamlReader::is_tag_present(yml, RECORDER_SPECS_MAX_PENDING_SAMPLES_TAG))
    {
        max_pending_samples = YamlReader::get<int>(yml, RECORDER_SPECS_MAX_PENDING_SAMPLES_TAG, version);
        if (max_pending_samples < -1)
        {
            throw eprosima::utils::ConfigurationException(
                      utils::Formatter() << "Error reading value under tag <" << RECORDER_SPECS_MAX_PENDING_SAMPLES_TAG <<
                          "> : value cannot be lower than -1.");
        }
    }

    // Get cleanup period
    if (YamlReader::is_tag_present(yml, RECORDER_SPECS_CLEANUP_PERIOD_TAG))
    {
        cleanup_period = YamlReader::get_positive_int(yml, RECORDER_SPECS_CLEANUP_PERIOD_TAG);
    }
}

void RecorderConfiguration::load_dds_configuration_(
        const Yaml& yml,
        const YamlReaderVersion& version)
{
    // Get optional DDS domain
    if (YamlReader::is_tag_present(yml, DOMAIN_ID_TAG))
    {
        simple_configuration->domain = YamlReader::get<DomainId>(yml, DOMAIN_ID_TAG, version);
    }

    /////
    // Get optional whitelist interfaces
    if (YamlReader::is_tag_present(yml, WHITELIST_INTERFACES_TAG))
    {
        simple_configuration->whitelist = YamlReader::get_set<IpType>(yml, WHITELIST_INTERFACES_TAG,
                        version);
    }

    // Optional get Transport protocol
    if (YamlReader::is_tag_present(yml, TRANSPORT_DESCRIPTORS_TRANSPORT_TAG))
    {
        simple_configuration->transport = YamlReader::get<TransportDescriptors>(yml,
                        TRANSPORT_DESCRIPTORS_TRANSPORT_TAG,
                        version);
    }
    else
    {
        simple_configuration->transport = TransportDescriptors::builtin;
    }

    // Optional get ignore participant flags
    if (YamlReader::is_tag_present(yml, IGNORE_PARTICIPANT_FLAGS_TAG))
    {
        simple_configuration->ignore_participant_flags = YamlReader::get<IgnoreParticipantFlags>(yml,
                        IGNORE_PARTICIPANT_FLAGS_TAG,
                        version);
    }
    else
    {
        simple_configuration->ignore_participant_flags = IgnoreParticipantFlags::no_filter;
    }

    /////
    // Get optional allowlist
    if (YamlReader::is_tag_present(yml, ALLOWLIST_TAG))
    {
        ddspipe_configuration.allowlist = YamlReader::get_set<utils::Heritable<IFilterTopic>>(yml, ALLOWLIST_TAG,
                        version);
    }

    /////
    // Get optional blocklist
    if (YamlReader::is_tag_present(yml, BLOCKLIST_TAG))
    {
        ddspipe_configuration.blocklist = YamlReader::get_set<utils::Heritable<IFilterTopic>>(yml, BLOCKLIST_TAG,
                        version);
    }

    /////
    // Get optional topics
    if (YamlReader::is_tag_present(yml, TOPICS_TAG))
    {
        const auto& manual_topics = YamlReader::get_list<ManualTopic>(yml, TOPICS_TAG, version);
        ddspipe_configuration.manual_topics =
                std::vector<ManualTopic>(manual_topics.begin(), manual_topics.end());
    }

    /////
    // Get optional builtin topics
    if (YamlReader::is_tag_present(yml, BUILTIN_TAG))
    {
        // WARNING: Parse builtin topics AFTER specs and recorder, as some topic-specific default values are set there
        ddspipe_configuration.builtin_topics = YamlReader::get_set<utils::Heritable<DistributedTopic>>(yml, BUILTIN_TAG,
                        version);
    }
}

void RecorderConfiguration::load_ddsrecorder_configuration_from_file_(
        const std::string& file_path)
{
    Yaml yml;

    // Load file
    try
    {
        if (!file_path.empty())
        {
            yml = YamlManager::load_file(file_path);
        }
    }
    catch (const std::exception& e)
    {
        throw eprosima::utils::ConfigurationException(
                  utils::Formatter() << "Error loading DDS Recorder configuration from file: <" << file_path <<
                      "> :\n " << e.what());
    }

    RecorderConfiguration::load_ddsrecorder_configuration_(yml);
}

} /* namespace yaml */
} /* namespace ddsrecorder */
} /* namespace eprosima */
