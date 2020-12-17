//
//  main.swift
//  RealmBootcamp
//
//  Created by Dominic Frei on 17/12/2020.
//

import RealmC

public struct Config {
    let cConfig: OpaquePointer
    public init() {
        self.cConfig = realm_config_new()
        let path = "test5.realm"
        let realmString = realm_string(data: strdup(path), size: path.count)
        realm_config_set_path(cConfig, realmString)
    }
}

let config = Config()

let className = realm_string(data: strdup("foo"), size: "foo".count)
let primaryKey = realm_string(data: strdup("x"), size: "x".count)
let classInfo = realm_class_info(name: className, primary_key: primaryKey, num_properties: 3, num_computed_properties: 0, key: realm_table_key(), flags: Int32(RLM_CLASS_NORMAL.rawValue))

let propertyName1 = realm_string(data: strdup("x"), size: "x".count)
let propertyName2 = realm_string(data: strdup("y"), size: "y".count)
let propertyName3 = realm_string(data: strdup("z"), size: "z".count)
let emptyString = realm_string(data: strdup(""), size: "".count)
var property1 = realm_property_info_t(name: propertyName1, public_name: emptyString, type: RLM_PROPERTY_TYPE_INT, collection_type: RLM_COLLECTION_TYPE_NONE, link_target: emptyString, link_origin_property_name: emptyString, key: realm_col_key(), flags: Int32(RLM_PROPERTY_NORMAL.rawValue))
var property2 = realm_property_info_t(name: propertyName2, public_name: emptyString, type: RLM_PROPERTY_TYPE_INT, collection_type: RLM_COLLECTION_TYPE_NONE, link_target: emptyString, link_origin_property_name: emptyString, key: realm_col_key(), flags: Int32(RLM_PROPERTY_NORMAL.rawValue))
var property3 = realm_property_info_t(name: propertyName3, public_name: emptyString, type: RLM_PROPERTY_TYPE_INT, collection_type: RLM_COLLECTION_TYPE_NONE, link_target: emptyString, link_origin_property_name: emptyString, key: realm_col_key(), flags: Int32(RLM_PROPERTY_NORMAL.rawValue))

var classProperties = [property1, property2, property3]
var unsafeBufferPointer = classProperties.withUnsafeBufferPointer({$0.baseAddress})
var unsafeMutablePointer = UnsafeMutablePointer<UnsafePointer<realm_property_info_t>?>.allocate(capacity: 1)
for index in 0...1 {
    unsafeMutablePointer.advanced(by: index).pointee = unsafeBufferPointer
}

var schema = realm_schema_new([classInfo], 1, unsafeMutablePointer)

realm_config_set_schema(config.cConfig, schema)
realm_config_set_schema_mode(config.cConfig, RLM_SCHEMA_MODE_AUTOMATIC)
realm_config_set_schema_version(config.cConfig, 1)

let realm = realm_open(config.cConfig)

var error = realm_error_t()
realm_get_last_error(&error)
print(String(cString: error.message.data))

