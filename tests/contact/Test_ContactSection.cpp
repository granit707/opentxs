// Copyright (c) 2010-2020 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "OTTestEnvironment.hpp"

namespace
{

class Test_ContactSection : public ::testing::Test
{
public:
    Test_ContactSection()
        : api_(dynamic_cast<const ot::api::client::internal::Manager&>(
              ot::Context().StartClient({}, 0)))
        , contactSection_(
              api_,
              std::string("testContactSectionNym1"),
              CONTACT_CONTACT_DATA_VERSION,
              CONTACT_CONTACT_DATA_VERSION,
              ot::proto::ContactSectionName::CONTACTSECTION_IDENTIFIER,
              ot::ContactSection::GroupMap{})
        , contactGroup_(new ot::ContactGroup(
              std::string("testContactGroupNym1"),
              ot::proto::ContactSectionName::CONTACTSECTION_IDENTIFIER,
              ot::proto::ContactItemType::CITEMTYPE_EMPLOYEE,
              {}))
        , activeContactItem_(new ot::ContactItem(
              api_,
              std::string("activeContactItem"),
              CONTACT_CONTACT_DATA_VERSION,
              CONTACT_CONTACT_DATA_VERSION,
              ot::proto::ContactSectionName::CONTACTSECTION_IDENTIFIER,
              ot::proto::ContactItemType::CITEMTYPE_EMPLOYEE,
              std::string("activeContactItemValue"),
              {ot::proto::ContactItemAttribute::CITEMATTR_ACTIVE},
              NULL_START,
              NULL_END,
              ""))
    {
    }

    const ot::api::client::internal::Manager& api_;
    const ot::ContactSection contactSection_;
    const std::shared_ptr<ot::ContactGroup> contactGroup_;
    const std::shared_ptr<ot::ContactItem> activeContactItem_;
};

}  // namespace

TEST_F(Test_ContactSection, first_constructor)
{
    const auto& group1 = std::shared_ptr<ot::ContactGroup>(
        new ot::ContactGroup(contactGroup_->AddItem(activeContactItem_)));
    ot::ContactSection::GroupMap groupMap{
        {ot::proto::ContactItemType::CITEMTYPE_EMPLOYEE, group1}};

    const ot::ContactSection section1(
        api_,
        "testContactSectionNym1",
        CONTACT_CONTACT_DATA_VERSION,
        CONTACT_CONTACT_DATA_VERSION,
        ot::proto::ContactSectionName::CONTACTSECTION_IDENTIFIER,
        groupMap);
    ASSERT_EQ(
        ot::proto::ContactSectionName::CONTACTSECTION_IDENTIFIER,
        section1.Type());
    ASSERT_EQ(CONTACT_CONTACT_DATA_VERSION, section1.Version());
    ASSERT_EQ(section1.Size(), 1);
    ASSERT_EQ(
        group1->Size(),
        section1.Group(ot::proto::ContactItemType::CITEMTYPE_EMPLOYEE)->Size());
    ASSERT_NE(section1.end(), section1.begin());
}

TEST_F(Test_ContactSection, first_constructor_different_versions)
{
    // Test private static method check_version.
    const ot::ContactSection section2(
        api_,
        "testContactSectionNym2",
        CONTACT_CONTACT_DATA_VERSION - 1,  // previous version
        CONTACT_CONTACT_DATA_VERSION,
        ot::proto::ContactSectionName::CONTACTSECTION_IDENTIFIER,
        ot::ContactSection::GroupMap{});
    ASSERT_EQ(CONTACT_CONTACT_DATA_VERSION, section2.Version());
}

TEST_F(Test_ContactSection, second_constructor)
{
    const ot::ContactSection section1(
        api_,
        "testContactSectionNym1",
        CONTACT_CONTACT_DATA_VERSION,
        CONTACT_CONTACT_DATA_VERSION,
        ot::proto::ContactSectionName::CONTACTSECTION_IDENTIFIER,
        activeContactItem_);
    ASSERT_EQ(
        ot::proto::ContactSectionName::CONTACTSECTION_IDENTIFIER,
        section1.Type());
    ASSERT_EQ(CONTACT_CONTACT_DATA_VERSION, section1.Version());
    ASSERT_EQ(section1.Size(), 1);
    ASSERT_NE(
        nullptr,
        section1.Group(ot::proto::ContactItemType::CITEMTYPE_EMPLOYEE));
    ASSERT_EQ(
        section1.Group(ot::proto::ContactItemType::CITEMTYPE_EMPLOYEE)->Size(),
        1);
    ASSERT_NE(section1.end(), section1.begin());
}

TEST_F(Test_ContactSection, third_constructor)
{
    ot::proto::ContactSection protoContactSection;
    protoContactSection.set_name(
        ot::proto::ContactSectionName::CONTACTSECTION_IDENTIFIER);
    protoContactSection.set_version(CONTACT_CONTACT_DATA_VERSION);
    //    ot::proto::ContactItem * item = protoContactSection.add_item();

    const ot::ContactSection section1(
        api_,
        "deserializedContactSectionNym1",
        CONTACT_CONTACT_DATA_VERSION,
        protoContactSection);
    ASSERT_EQ(
        ot::proto::ContactSectionName::CONTACTSECTION_IDENTIFIER,
        section1.Type());
    ASSERT_EQ(CONTACT_CONTACT_DATA_VERSION, section1.Version());
}

TEST_F(Test_ContactSection, copy_constructor)
{
    const auto& section1(contactSection_.AddItem(activeContactItem_));

    ot::ContactSection copiedContactSection(section1);
    ASSERT_EQ(section1.Type(), copiedContactSection.Type());
    ASSERT_EQ(section1.Version(), copiedContactSection.Version());
    ASSERT_EQ(copiedContactSection.Size(), 1);
    ASSERT_NE(
        nullptr,
        copiedContactSection.Group(
            ot::proto::ContactItemType::CITEMTYPE_EMPLOYEE));
    ASSERT_EQ(
        copiedContactSection
            .Group(ot::proto::ContactItemType::CITEMTYPE_EMPLOYEE)
            ->Size(),
        1);
    ASSERT_NE(copiedContactSection.end(), copiedContactSection.begin());
}

TEST_F(Test_ContactSection, operator_plus)
{
    // Combine two sections with one item each of the same type.
    const auto& section1 = contactSection_.AddItem(activeContactItem_);

    const std::shared_ptr<ot::ContactItem> contactItem2(new ot::ContactItem(
        api_,
        std::string("activeContactItem2"),
        CONTACT_CONTACT_DATA_VERSION,
        CONTACT_CONTACT_DATA_VERSION,
        ot::proto::ContactSectionName::CONTACTSECTION_IDENTIFIER,
        ot::proto::ContactItemType::CITEMTYPE_EMPLOYEE,
        std::string("activeContactItemValue2"),
        {ot::proto::ContactItemAttribute::CITEMATTR_ACTIVE},
        NULL_START,
        NULL_END,
        ""));
    const ot::ContactSection section2(
        api_,
        "testContactSectionNym2",
        CONTACT_CONTACT_DATA_VERSION,
        CONTACT_CONTACT_DATA_VERSION,
        ot::proto::ContactSectionName::CONTACTSECTION_IDENTIFIER,
        contactItem2);

    const auto& section3 = section1 + section2;
    // Verify the section has one group.
    ASSERT_EQ(section3.Size(), 1);
    // Verify the group has two items.
    ASSERT_EQ(
        section3.Group(ot::proto::ContactItemType::CITEMTYPE_EMPLOYEE)->Size(),
        2);

    // Add a section that has one group with one item of the same type, and
    // another group with one item of a different type.
    const std::shared_ptr<ot::ContactItem> contactItem3(new ot::ContactItem(
        api_,
        std::string("activeContactItem3"),
        CONTACT_CONTACT_DATA_VERSION,
        CONTACT_CONTACT_DATA_VERSION,
        ot::proto::ContactSectionName::CONTACTSECTION_IDENTIFIER,
        ot::proto::ContactItemType::CITEMTYPE_EMPLOYEE,
        std::string("activeContactItemValue3"),
        {ot::proto::ContactItemAttribute::CITEMATTR_ACTIVE},
        NULL_START,
        NULL_END,
        ""));
    const std::shared_ptr<ot::ContactItem> contactItem4(new ot::ContactItem(
        api_,
        std::string("activeContactItem4"),
        CONTACT_CONTACT_DATA_VERSION,
        CONTACT_CONTACT_DATA_VERSION,
        ot::proto::ContactSectionName::CONTACTSECTION_IDENTIFIER,
        ot::proto::ContactItemType::CITEMTYPE_SSL,
        std::string("activeContactItemValue4"),
        {ot::proto::ContactItemAttribute::CITEMATTR_ACTIVE},
        NULL_START,
        NULL_END,
        ""));
    const ot::ContactSection section4(
        api_,
        "testContactSectionNym4",
        CONTACT_CONTACT_DATA_VERSION,
        CONTACT_CONTACT_DATA_VERSION,
        ot::proto::ContactSectionName::CONTACTSECTION_IDENTIFIER,
        contactItem3);
    const auto& section5 = section4.AddItem(contactItem4);

    const auto& section6 = section3 + section5;
    // Verify the section has two groups.
    ASSERT_EQ(section6.Size(), 2);
    // Verify the first group has three items.
    ASSERT_EQ(
        section6.Group(ot::proto::ContactItemType::CITEMTYPE_EMPLOYEE)->Size(),
        3);
    // Verify the second group has one item.
    ASSERT_EQ(
        section6.Group(ot::proto::ContactItemType::CITEMTYPE_SSL)->Size(), 1);
}

TEST_F(Test_ContactSection, operator_plus_different_versions)
{
    // rhs version less than lhs
    const ot::ContactSection section2(
        api_,
        "testContactSectionNym2",
        CONTACT_CONTACT_DATA_VERSION - 1,
        CONTACT_CONTACT_DATA_VERSION - 1,
        ot::proto::ContactSectionName::CONTACTSECTION_IDENTIFIER,
        ot::ContactSection::GroupMap{});

    const auto& section3 = contactSection_ + section2;
    // Verify the new section has the latest version.
    ASSERT_EQ(CONTACT_CONTACT_DATA_VERSION, section3.Version());

    // lhs version less than rhs
    const auto& section4 = section2 + contactSection_;
    // Verify the new section has the latest version.
    ASSERT_EQ(CONTACT_CONTACT_DATA_VERSION, section4.Version());
}

TEST_F(Test_ContactSection, AddItem)
{
    // Add an item to a SCOPE section.
    const std::shared_ptr<ot::ContactItem> scopeContactItem(new ot::ContactItem(
        api_,
        std::string("scopeContactItem"),
        CONTACT_CONTACT_DATA_VERSION,
        CONTACT_CONTACT_DATA_VERSION,
        ot::proto::ContactSectionName::CONTACTSECTION_SCOPE,
        ot::proto::ContactItemType::CITEMTYPE_INDIVIDUAL,
        std::string("scopeContactItemValue"),
        {ot::proto::ContactItemAttribute::CITEMATTR_LOCAL},
        NULL_START,
        NULL_END,
        ""));
    const ot::ContactSection section1(
        api_,
        "testContactSectionNym2",
        CONTACT_CONTACT_DATA_VERSION,
        CONTACT_CONTACT_DATA_VERSION,
        ot::proto::ContactSectionName::CONTACTSECTION_SCOPE,
        ot::ContactSection::GroupMap{});
    const auto& section2 = section1.AddItem(scopeContactItem);
    ASSERT_EQ(section2.Size(), 1);
    ASSERT_EQ(
        section2.Group(ot::proto::ContactItemType::CITEMTYPE_INDIVIDUAL)
            ->Size(),
        1);
    ASSERT_TRUE(section2.Claim(scopeContactItem->ID())->isPrimary());
    ASSERT_TRUE(section2.Claim(scopeContactItem->ID())->isActive());

    // Add an item to a non-scope section.
    const auto& section4 = contactSection_.AddItem(activeContactItem_);
    ASSERT_EQ(section4.Size(), 1);
    ASSERT_EQ(
        section4.Group(ot::proto::ContactItemType::CITEMTYPE_EMPLOYEE)->Size(),
        1);
    // Add a second item of the same type.
    const std::shared_ptr<ot::ContactItem> contactItem2(new ot::ContactItem(
        api_,
        std::string("activeContactItem2"),
        CONTACT_CONTACT_DATA_VERSION,
        CONTACT_CONTACT_DATA_VERSION,
        ot::proto::ContactSectionName::CONTACTSECTION_IDENTIFIER,
        ot::proto::ContactItemType::CITEMTYPE_EMPLOYEE,
        std::string("activeContactItemValue2"),
        {ot::proto::ContactItemAttribute::CITEMATTR_ACTIVE},
        NULL_START,
        NULL_END,
        ""));
    const auto& section5 = section4.AddItem(contactItem2);
    // Verify there are two items.
    ASSERT_EQ(section5.Size(), 1);
    ASSERT_EQ(
        section5.Group(ot::proto::ContactItemType::CITEMTYPE_EMPLOYEE)->Size(),
        2);

    // Add an item of a different type.
    const std::shared_ptr<ot::ContactItem> contactItem3(new ot::ContactItem(
        api_,
        std::string("activeContactItem3"),
        CONTACT_CONTACT_DATA_VERSION,
        CONTACT_CONTACT_DATA_VERSION,
        ot::proto::ContactSectionName::CONTACTSECTION_IDENTIFIER,
        ot::proto::ContactItemType::CITEMTYPE_SSL,
        std::string("activeContactItemValue3"),
        {ot::proto::ContactItemAttribute::CITEMATTR_ACTIVE},
        NULL_START,
        NULL_END,
        ""));
    const auto& section6 = section5.AddItem(contactItem3);
    // Verify there are two groups.
    ASSERT_EQ(section6.Size(), 2);
    // Verify there is one in the second group.
    ASSERT_EQ(
        section6.Group(ot::proto::ContactItemType::CITEMTYPE_SSL)->Size(), 1);
}

TEST_F(Test_ContactSection, AddItem_different_versions)
{
    // Add an item with a newer version to a SCOPE section.
    const std::shared_ptr<ot::ContactItem> scopeContactItem(new ot::ContactItem(
        api_,
        std::string("scopeContactItem"),
        CONTACT_CONTACT_DATA_VERSION,
        CONTACT_CONTACT_DATA_VERSION,
        ot::proto::ContactSectionName::CONTACTSECTION_SCOPE,
        ot::proto::ContactItemType::CITEMTYPE_BOT,
        std::string("scopeContactItemValue"),
        {ot::proto::ContactItemAttribute::CITEMATTR_LOCAL},
        NULL_START,
        NULL_END,
        ""));
    const ot::ContactSection section1(
        api_,
        "testContactSectionNym2",
        3,  // version of CONTACTSECTION_SCOPE section before CITEMTYPE_BOT was
            // added
        3,
        ot::proto::ContactSectionName::CONTACTSECTION_SCOPE,
        ot::ContactSection::GroupMap{});
    const auto& section2 = section1.AddItem(scopeContactItem);
    ASSERT_EQ(section2.Size(), 1);
    ASSERT_EQ(
        section2.Group(ot::proto::ContactItemType::CITEMTYPE_BOT)->Size(), 1);
    ASSERT_TRUE(section2.Claim(scopeContactItem->ID())->isPrimary());
    ASSERT_TRUE(section2.Claim(scopeContactItem->ID())->isActive());
    // Verify the section version has been updated to the minimum version to
    // support CITEMTYPE_BOT.
    ASSERT_EQ(section2.Version(), 4);

    // Add an item with a newer version to a non-scope section.
    const std::shared_ptr<ot::ContactItem> contactItem2(new ot::ContactItem(
        api_,
        std::string("contactItem2"),
        CONTACT_CONTACT_DATA_VERSION,
        CONTACT_CONTACT_DATA_VERSION,
        ot::proto::ContactSectionName::CONTACTSECTION_RELATIONSHIP,
        ot::proto::ContactItemType::CITEMTYPE_OWNER,
        std::string("contactItem2Value"),
        {ot::proto::ContactItemAttribute::CITEMATTR_LOCAL},
        NULL_START,
        NULL_END,
        ""));
    const ot::ContactSection section3(
        api_,
        "testContactSectionNym3",
        3,  // version of CONTACTSECTION_RELATIONSHIP section before
            // CITEMTYPE_OWNER was added
        3,
        ot::proto::ContactSectionName::CONTACTSECTION_RELATIONSHIP,
        ot::ContactSection::GroupMap{});
    const auto& section4 = section3.AddItem(contactItem2);
    ASSERT_EQ(section4.Size(), 1);
    ASSERT_EQ(
        section4.Group(ot::proto::ContactItemType::CITEMTYPE_OWNER)->Size(), 1);
    // Verify the section version has been updated to the minimum version to
    // support CITEMTYPE_OWNER.
    ASSERT_EQ(4, section4.Version());
}

TEST_F(Test_ContactSection, begin)
{
    ot::ContactSection::GroupMap::const_iterator it = contactSection_.begin();
    ASSERT_EQ(contactSection_.end(), it);
    ASSERT_EQ(std::distance(it, contactSection_.end()), 0);

    const auto& section1 = contactSection_.AddItem(activeContactItem_);
    it = section1.begin();
    ASSERT_NE(section1.end(), it);
    ASSERT_EQ(std::distance(it, section1.end()), 1);

    std::advance(it, 1);
    ASSERT_EQ(section1.end(), it);
    ASSERT_EQ(std::distance(it, section1.end()), 0);
}

TEST_F(Test_ContactSection, Claim_found)
{
    const auto& section1 = contactSection_.AddItem(activeContactItem_);

    const std::shared_ptr<ot::ContactItem>& claim =
        section1.Claim(activeContactItem_->ID());
    ASSERT_NE(nullptr, claim);
    ASSERT_EQ(activeContactItem_->ID(), claim->ID());

    // Find a claim in a different group.
    const std::shared_ptr<ot::ContactItem> contactItem2(new ot::ContactItem(
        api_,
        std::string("activeContactItem2"),
        CONTACT_CONTACT_DATA_VERSION,
        CONTACT_CONTACT_DATA_VERSION,
        ot::proto::ContactSectionName::CONTACTSECTION_IDENTIFIER,
        ot::proto::ContactItemType::CITEMTYPE_SSL,
        std::string("activeContactItemValue2"),
        {ot::proto::ContactItemAttribute::CITEMATTR_ACTIVE},
        NULL_START,
        NULL_END,
        ""));
    const auto& section2 = section1.AddItem(contactItem2);
    const std::shared_ptr<ot::ContactItem>& claim2 =
        section2.Claim(contactItem2->ID());
    ASSERT_NE(nullptr, claim2);
    ASSERT_EQ(contactItem2->ID(), claim2->ID());
}

TEST_F(Test_ContactSection, Claim_notfound)
{
    const std::shared_ptr<ot::ContactItem>& claim =
        contactSection_.Claim(activeContactItem_->ID());
    ASSERT_FALSE(claim);
}

TEST_F(Test_ContactSection, end)
{
    ot::ContactSection::GroupMap::const_iterator it = contactSection_.end();
    ASSERT_EQ(contactSection_.begin(), it);
    ASSERT_EQ(std::distance(contactSection_.begin(), it), 0);

    const auto& section1 = contactSection_.AddItem(activeContactItem_);
    it = section1.end();
    ASSERT_NE(section1.begin(), it);
    ASSERT_EQ(std::distance(section1.begin(), it), 1);

    std::advance(it, -1);
    ASSERT_EQ(section1.begin(), it);
    ASSERT_EQ(std::distance(section1.begin(), it), 0);
}

TEST_F(Test_ContactSection, Group_found)
{
    const auto& section1 = contactSection_.AddItem(activeContactItem_);
    ASSERT_NE(
        nullptr,
        section1.Group(ot::proto::ContactItemType::CITEMTYPE_EMPLOYEE));
}

TEST_F(Test_ContactSection, Group_notfound)
{
    ASSERT_FALSE(
        contactSection_.Group(ot::proto::ContactItemType::CITEMTYPE_EMPLOYEE));
}

TEST_F(Test_ContactSection, HaveClaim_true)
{
    const auto& section1 = contactSection_.AddItem(activeContactItem_);

    ASSERT_TRUE(section1.HaveClaim(activeContactItem_->ID()));

    // Find a claim in a different group.
    const std::shared_ptr<ot::ContactItem> contactItem2(new ot::ContactItem(
        api_,
        std::string("activeContactItem2"),
        CONTACT_CONTACT_DATA_VERSION,
        CONTACT_CONTACT_DATA_VERSION,
        ot::proto::ContactSectionName::CONTACTSECTION_IDENTIFIER,
        ot::proto::ContactItemType::CITEMTYPE_SSL,
        std::string("activeContactItemValue2"),
        {ot::proto::ContactItemAttribute::CITEMATTR_ACTIVE},
        NULL_START,
        NULL_END,
        ""));
    const auto& section2 = section1.AddItem(contactItem2);
    ASSERT_TRUE(section2.HaveClaim(contactItem2->ID()));
}

TEST_F(Test_ContactSection, HaveClaim_false)
{
    ASSERT_FALSE(contactSection_.HaveClaim(activeContactItem_->ID()));
}

TEST_F(Test_ContactSection, Delete)
{
    const auto& section1 = contactSection_.AddItem(activeContactItem_);
    ASSERT_TRUE(section1.HaveClaim(activeContactItem_->ID()));

    // Add a second item to help testing the size after trying to delete twice.
    const std::shared_ptr<ot::ContactItem> contactItem2(new ot::ContactItem(
        api_,
        std::string("activeContactItem2"),
        CONTACT_CONTACT_DATA_VERSION,
        CONTACT_CONTACT_DATA_VERSION,
        ot::proto::ContactSectionName::CONTACTSECTION_IDENTIFIER,
        ot::proto::ContactItemType::CITEMTYPE_EMPLOYEE,
        std::string("activeContactItemValue2"),
        {ot::proto::ContactItemAttribute::CITEMATTR_ACTIVE},
        NULL_START,
        NULL_END,
        ""));
    const auto& section2 = section1.AddItem(contactItem2);
    ASSERT_EQ(
        section2.Group(ot::proto::ContactItemType::CITEMTYPE_EMPLOYEE)->Size(),
        2);

    const auto& section3 = section2.Delete(activeContactItem_->ID());
    // Verify the item was deleted.
    ASSERT_FALSE(section3.HaveClaim(activeContactItem_->ID()));
    ASSERT_EQ(
        section3.Group(ot::proto::ContactItemType::CITEMTYPE_EMPLOYEE)->Size(),
        1);

    const auto& section4 = section3.Delete(activeContactItem_->ID());
    // Verify trying to delete the item again didn't change anything.
    ASSERT_EQ(
        section4.Group(ot::proto::ContactItemType::CITEMTYPE_EMPLOYEE)->Size(),
        1);

    // Add an item of a different type.
    const std::shared_ptr<ot::ContactItem> contactItem3(new ot::ContactItem(
        api_,
        std::string("activeContactItem3"),
        CONTACT_CONTACT_DATA_VERSION,
        CONTACT_CONTACT_DATA_VERSION,
        ot::proto::ContactSectionName::CONTACTSECTION_IDENTIFIER,
        ot::proto::ContactItemType::CITEMTYPE_SSL,
        std::string("activeContactItemValue3"),
        {ot::proto::ContactItemAttribute::CITEMATTR_ACTIVE},
        NULL_START,
        NULL_END,
        ""));
    const auto& section5 = section4.AddItem(contactItem3);
    // Verify the section has two groups.
    ASSERT_EQ(section5.Size(), 2);
    ASSERT_EQ(
        section5.Group(ot::proto::ContactItemType::CITEMTYPE_SSL)->Size(), 1);
    ASSERT_TRUE(section5.HaveClaim(contactItem3->ID()));

    const auto& section6 = section5.Delete(contactItem3->ID());
    // Verify the item was deleted and the group was removed.
    ASSERT_EQ(section6.Size(), 1);
    ASSERT_FALSE(section6.HaveClaim(contactItem3->ID()));
    ASSERT_FALSE(section6.Group(ot::proto::ContactItemType::CITEMTYPE_SSL));
}

TEST_F(Test_ContactSection, SerializeTo)
{
    // Serialize without ids.
    ot::proto::ContactData contactData1;

    const auto& section1 = contactSection_.AddItem(activeContactItem_);
    ASSERT_TRUE(section1.SerializeTo(contactData1, false));
    ASSERT_EQ(section1.Size(), contactData1.section_size());

    ot::proto::ContactSection contactDataSection = contactData1.section(0);
    ASSERT_EQ(
        ot::proto::ContactSectionName::CONTACTSECTION_IDENTIFIER,
        contactDataSection.name());

    ot::proto::ContactItem contactDataItem = contactDataSection.item(0);
    ASSERT_EQ(activeContactItem_->Value(), contactDataItem.value());
    ASSERT_EQ(activeContactItem_->Version(), contactDataItem.version());
    ASSERT_EQ(activeContactItem_->Type(), contactDataItem.type());
    ASSERT_EQ(activeContactItem_->Start(), contactDataItem.start());
    ASSERT_EQ(activeContactItem_->End(), contactDataItem.end());

    // Serialize with ids.
    ot::proto::ContactData contactData2;

    ASSERT_TRUE(section1.SerializeTo(contactData2, true));
    ASSERT_EQ(section1.Size(), contactData2.section_size());

    contactDataSection = contactData2.section(0);
    ASSERT_EQ(
        ot::proto::ContactSectionName::CONTACTSECTION_IDENTIFIER,
        contactDataSection.name());

    contactDataItem = contactDataSection.item(0);

    ASSERT_EQ(activeContactItem_->ID().str(), contactDataItem.id());
    ASSERT_EQ(activeContactItem_->Value(), contactDataItem.value());
    ASSERT_EQ(activeContactItem_->Version(), contactDataItem.version());
    ASSERT_EQ(activeContactItem_->Type(), contactDataItem.type());
    ASSERT_EQ(activeContactItem_->Start(), contactDataItem.start());
    ASSERT_EQ(activeContactItem_->End(), contactDataItem.end());
}

TEST_F(Test_ContactSection, Size)
{
    ASSERT_EQ(contactSection_.Size(), 0);
    const auto& section1 = contactSection_.AddItem(activeContactItem_);
    ASSERT_EQ(section1.Size(), 1);

    // Add a second item of the same type.
    const std::shared_ptr<ot::ContactItem> contactItem2(new ot::ContactItem(
        api_,
        std::string("activeContactItem2"),
        CONTACT_CONTACT_DATA_VERSION,
        CONTACT_CONTACT_DATA_VERSION,
        ot::proto::ContactSectionName::CONTACTSECTION_IDENTIFIER,
        ot::proto::ContactItemType::CITEMTYPE_EMPLOYEE,
        std::string("activeContactItemValue2"),
        {ot::proto::ContactItemAttribute::CITEMATTR_ACTIVE},
        NULL_START,
        NULL_END,
        ""));
    const auto& section2 = section1.AddItem(contactItem2);
    // Verify the size is the same.
    ASSERT_EQ(section2.Size(), 1);

    // Add an item of a different type.
    const std::shared_ptr<ot::ContactItem> contactItem3(new ot::ContactItem(
        api_,
        std::string("activeContactItem3"),
        CONTACT_CONTACT_DATA_VERSION,
        CONTACT_CONTACT_DATA_VERSION,
        ot::proto::ContactSectionName::CONTACTSECTION_IDENTIFIER,
        ot::proto::ContactItemType::CITEMTYPE_SSL,
        std::string("activeContactItemValue3"),
        {ot::proto::ContactItemAttribute::CITEMATTR_ACTIVE},
        NULL_START,
        NULL_END,
        ""));
    const auto& section3 = section2.AddItem(contactItem3);
    // Verify the size is now two.
    ASSERT_EQ(section3.Size(), 2);

    // Delete an item from the first group.
    const auto& section4 = section3.Delete(contactItem2->ID());
    // Verify the size is still two.
    ASSERT_EQ(section4.Size(), 2);

    // Delete the item from the second group.
    const auto& section5 = section4.Delete(contactItem3->ID());
    // Verify that the size is now one.
    ASSERT_EQ(section5.Size(), 1);
}

TEST_F(Test_ContactSection, Type)
{
    ASSERT_EQ(
        ot::proto::ContactSectionName::CONTACTSECTION_IDENTIFIER,
        contactSection_.Type());
}

TEST_F(Test_ContactSection, Version)
{
    ASSERT_EQ(CONTACT_CONTACT_DATA_VERSION, contactSection_.Version());
}
