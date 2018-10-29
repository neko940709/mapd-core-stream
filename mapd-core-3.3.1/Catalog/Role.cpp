/*
 * File:   Role.cpp
 * Author: norair
 *
 * Created on May 16, 2017, 03:30 PM
 */

#include "Role.h"

using std::runtime_error;
using std::string;

//      ***** Class Role *****

Role::Role(const std::string& name) : roleName_(name) {}

Role::Role(const Role& role) : roleName_(role.roleName_) {
  copyDbObjects(role);  // does deep copy
}

Role::~Role() {
  for (auto dbObjectIt = dbObjectMap_.begin(); dbObjectIt != dbObjectMap_.end(); ++dbObjectIt) {
    delete dbObjectIt->second;
  }
  dbObjectMap_.clear();
}

const Role::DBObjectMap* Role::getDbObject() const {
  return &dbObjectMap_;
}

DBObject* Role::findDbObject(const DBObjectKey objectKey) const {
  DBObject* dbObject = nullptr;
  auto dbObjectIt = dbObjectMap_.find(objectKey);
  if (dbObjectIt != dbObjectMap_.end()) {
    dbObject = dbObjectIt->second;
  }
  return dbObject;
}

void Role::copyDbObjects(const Role& role) {
  for (auto it = role.dbObjectMap_.begin(); it != role.dbObjectMap_.end(); ++it) {
    dbObjectMap_[it->first] = new DBObject(*(it->second));
  }
}

//      ***** Class UserRole *****

/*
 * a) when User just being created (this is a key) and granted a Role, its <groupRole_> set is empty, so in next
 * constructor,
 *    new Role just needs to be added to the set <groupRole_> for that new User.
 * b) in case when User already exists and it is just being granted new/another Role, that new Role needs to be added to
 *    the <groupRole_> set of the User;
 * c) when copy constructor used (see second constructor below: "UserRole::UserRole(const UserRole& role)"), the
 * <groupRole_>
 *    set should be deep copied to new UserRole object because each UserRole object can have it's own set of
 * Roles/Groups
 *    after all, and they can change independently from the original UserRole object it was copied from.
 */
// this is the main constructor which is called when executing GRANT Role to User command
UserRole::UserRole(Role* role, const int32_t userId, const std::string& userName)
    : Role(*role), userId_(userId), userName_(userName) {}

UserRole::UserRole(const UserRole& role) : Role(role), userId_(role.userId_), userName_(role.userName_) {
  copyRoles(role.groupRole_);  // copy all pointers of <groupRole_> set from the from_ object to the to_ object
}

/*   Here are the actions which need to be taken in this destructor "UserRole::~UserRole()":
 * - parent destructor "Role::~Role()" will be called automatically to free all DbObjects related to this UserRole;
 * - need to go thru <groupRole_> set and for each element of the set delete current UserRole (i.e. this ptr);
 * - need to free/erase <groupRole_> set itself, don't need to delete each of the GroupRole* objects!!!
 * - to avoid circular deletion between GroupRole and UserRole here is what can be done:
 *   a) the destructor "UserRole::~UserRole()" should be called when User with the given "userName_" is being deleted;
 *   b) in cases when GroupRole is being deleted, which means all UserRole object belonging to that GroupRole, needs to
 * be re-evaluated,
 *      "deleteRole(const Role* role)" api should be called which will:
 *      - first of all delete corresponding GroupRole from the <groupRole_> set;
 *      - invalidate and recalculate all DbObjects for this UserRole as needed (may be see api
 * "updatePrivileges(role)");
 *      - some other actions may be done as well;
 */

UserRole::~UserRole() {
  for (auto roleIt = groupRole_.begin(); roleIt != groupRole_.end(); ++roleIt) {
    (*roleIt)->removeRole(this);
  }
  groupRole_.clear();
}

size_t UserRole::getMembershipSize() const {
  return groupRole_.size();
}

bool UserRole::checkPrivileges(const DBObject& objectRequested) const {
  // dbObjectRequested describes DBObject and privileges requested for that DBObject

  bool rc =
      false;  // returns false if requested privileges are not granted or no privileges granted at all to this DBObject
  DBObjectKey objectKey = objectRequested.objectKey_;
  while (objectKey.size() > 1) {
    if (objectKey.size() == 2) {
      objectKey[0] = static_cast<int32_t>(DatabaseDBObjectType);
    }
    auto dbObject = findDbObject(objectKey);
    if (dbObject) {  // if object not found return false, i.e. no privileges for this object granted at all
      // User has access privileges to the object decsribed by dbObjectRequested objectId and type. Check to make sure
      // they are not violated.
      if (!dbObject->privsValid_) {
        CHECK(false);
      }
      rc = true;
      if ((objectRequested.objectPrivs_.select_ && !dbObject->objectPrivs_.select_) ||
          (objectRequested.objectPrivs_.insert_ && !dbObject->objectPrivs_.insert_) ||
          (objectRequested.objectPrivs_.create_ && !dbObject->objectPrivs_.create_) ||
          (objectRequested.objectPrivs_.truncate_ && !dbObject->objectPrivs_.truncate_)) {
        rc = false;
      }
    }
    if (rc) {
      break;
    }
    objectKey.pop_back();  // check if privileges set for higher level DBObject (ex. DB for Table)
  }

  return rc;
}

void UserRole::grantPrivileges(const DBObject& object) {
  // used for create_table and CTAS commands only, called from createDBObject() api
  auto dbObject = findDbObject(object.getObjectKey());
  if (!dbObject) {
    dbObjectMap_[object.getObjectKey()] = new DBObject(object);
  } else {  // found
    dbObject->grantPrivileges(object);
  }
}

void UserRole::revokePrivileges(const DBObject& object) {
  throw runtime_error("revokePrivileges() api should not be used with objects of the UserRole class.");
}

void UserRole::getPrivileges(DBObject& object) {
  throw runtime_error("getPrivileges() api should not be used with objects of the UserRole class.");
}

void UserRole::grantRole(Role* role) {
  addRole(role);
  role->grantRole(this);
  updatePrivileges();
}

void UserRole::revokeRole(Role* role) {
  groupRole_.erase(role);
  role->revokeRole(this);
  updatePrivileges();
}

bool UserRole::hasRole(Role* role) {
  bool found = false;
  for (auto roleIt = groupRole_.begin(); roleIt != groupRole_.end(); ++roleIt) {
    if (role == (*roleIt)) {
      found = true;
      break;
    }
  }
  return found;
}

void UserRole::updatePrivileges(Role* role) {
  for (auto dbObjectIt = role->getDbObject()->begin(); dbObjectIt != role->getDbObject()->end(); ++dbObjectIt) {
    auto dbObject = findDbObject(dbObjectIt->first);
    if (dbObject) {  // found
      if (dbObject->privsValid_) {
        dbObject->updatePrivileges(*(dbObjectIt->second));
      } else {
        dbObject->copyPrivileges(*(dbObjectIt->second));
      }
    } else {  // not found
      dbObjectMap_[dbObjectIt->first] = new DBObject(*(dbObjectIt->second));
    }
  }
}

void UserRole::updatePrivileges() {
  for (auto dbObjectIt = dbObjectMap_.begin(); dbObjectIt != dbObjectMap_.end(); ++dbObjectIt) {
    dbObjectIt->second->privsValid_ = false;
  }
  for (auto roleIt = groupRole_.begin(); roleIt != groupRole_.end(); ++roleIt) {
    if ((*roleIt)->getDbObject()->size() > 0) {
      updatePrivileges(*roleIt);
    }
  }
  for (auto dbObjectIt = dbObjectMap_.begin(); dbObjectIt != dbObjectMap_.end(); ++dbObjectIt) {
    if (dbObjectIt->second->privsValid_ == false) {
      dbObjectMap_.erase(dbObjectIt);
    }
  }
}

void UserRole::copyRoles(const std::unordered_set<Role*>& roles) {
  for (auto roleIt = roles.begin(); roleIt != roles.end(); ++roleIt) {
    groupRole_.insert(*roleIt);
  }
}

void UserRole::addRole(Role* role) {
  bool found = false;
  for (auto roleIt = groupRole_.begin(); roleIt != groupRole_.end(); ++roleIt) {
    if (role == (*roleIt)) {
      found = true;
      break;
    }
  }
  if (found) {
    throw runtime_error("Role " + role->roleName() + " have been granted to user " + userName_ + " already.");
  }
  groupRole_.insert(role);
}

void UserRole::removeRole(Role* role) {
  groupRole_.erase(role);
}

std::string UserRole::roleName(bool userName) const {
  if (userName) {
    return (roleName_ + "_" + userName_);
  } else {
    return (roleName_);
  }
}

bool UserRole::isUserPrivateRole() const {
  throw runtime_error("isUserPrivateRole() api should not be used with objects of the UserRole class.");
}

//      ***** Class GroupRole *****

GroupRole::GroupRole(const std::string& name, const bool& userPrivateRole)
    : Role(name), userPrivateRole_(userPrivateRole) {}

GroupRole::GroupRole(const GroupRole& role) : Role(role), userPrivateRole_(role.userPrivateRole_) {
  copyRoles(role.userRole_);  // copy all pointers of <userRole_> set from the from_ object to the to_ object
}

/*  Here are the actions which need to be done in this destructor:
 * - destructor to Role object will be called automatically, and so all DBObjects as well as <dbObject_> set
 *   will be deleted/cleared automatically;
 * - need to call "deleteRole(role)" api for all UserRoles from the <userRole_> set so this GroupRole will be
 *   deleted from the corresponding <groupRole_> set of the UserRole object and the privileges of that UserRole
 *   object will be adjusted as needed.
 * - need to "clear" the set "userRole_" itself (userRole_.clear()), without physically calling delete for the objects
 * of this set.
 */
GroupRole::~GroupRole() {
  for (auto roleIt = userRole_.begin(); roleIt != userRole_.end(); ++roleIt) {
    (*roleIt)->removeRole(this);
    (*roleIt)->updatePrivileges();
  }
  userRole_.clear();
}

size_t GroupRole::getMembershipSize() const {
  return userRole_.size();
}

bool GroupRole::checkPrivileges(const DBObject& objectRequested) const {
  throw runtime_error("checkPrivileges api should not be used with objects of the GroupRole class.");
}
void GroupRole::copyRoles(const std::unordered_set<Role*>& roles) {
  for (auto roleIt = roles.begin(); roleIt != roles.end(); ++roleIt) {
    userRole_.insert(*roleIt);
  }
}

void GroupRole::addRole(Role* role) {
  userRole_.insert(role);
}

void GroupRole::removeRole(Role* role) {
  revokeRole(role);
}

std::string GroupRole::roleName(bool userName) const {
  return roleName_;
}

void GroupRole::grantPrivileges(const DBObject& object) {
  DBObject* dbObject = findDbObject(object.getObjectKey());
  if (!dbObject) {  // not found
    dbObjectMap_[object.getObjectKey()] = new DBObject(object);
  } else {  // found
    dbObject->grantPrivileges(object);
  }
  updatePrivileges();
}

void GroupRole::revokePrivileges(const DBObject& object) {
  auto dbObject = findDbObject(object.getObjectKey());
  if (!dbObject || !dbObject->hasActivePrivs()) {  // not found or has none of privileges set
    throw runtime_error("Can not revoke privileges because " + roleName() + " has no privileges to " +
                        object.getName());
  }
  dbObject->revokePrivileges(object);
  updatePrivileges();
}

bool GroupRole::hasRole(Role* role) {
  throw runtime_error("hasRole() api should not be used with objects of the GroupRole class.");
}

void GroupRole::getPrivileges(DBObject& object) {
  auto dbObject = findDbObject(object.getObjectKey());
  if (!dbObject) {  // not found
    throw runtime_error("Can not get privileges because " + roleName() + " has no privileges to " + object.getName());
  }
  object.grantPrivileges(*dbObject);
}

void GroupRole::grantRole(Role* role) {
  addRole(role);
}

void GroupRole::revokeRole(Role* role) {
  userRole_.erase(role);
}

void GroupRole::updatePrivileges(Role* role) {
  throw runtime_error("updatePrivileges(Role*) api should not be used with objects of the GroupRole class.");
}

void GroupRole::updatePrivileges() {
  for (auto roleIt = userRole_.begin(); roleIt != userRole_.end(); ++roleIt) {
    (*roleIt)->updatePrivileges();
  }
}

bool GroupRole::isUserPrivateRole() const {
  return userPrivateRole_;
}
