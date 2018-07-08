// Copyright 2018 Phyronnaz

#include "VoxelWorldGeneratorPickerCustomization.h"

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "PropertyHandle.h"
#include "IDetailChildrenBuilder.h"
#include "DetailLayoutBuilder.h"
#include "PropertyCustomizationHelpers.h"
#include "ComboBox.h"

#include "PropertyEditorModule.h"
#include "ModuleManager.h"
#include "VoxelWorldGenerator.h"

#define LOCTEXT_NAMESPACE "VoxelWorldGeneratorPickerCustomization"

void FVoxelWorldGeneratorPickerCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	PropertyHandle = InPropertyHandle;

	ClassHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FVoxelWorldGeneratorPicker, WorldGeneratorClass));
	ObjectHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FVoxelWorldGeneratorPicker, WorldGeneratorObject));
	ClassOrObjectHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FVoxelWorldGeneratorPicker, UseClassOrObject));

	ClassOrObjectArray.Reset();
	ClassOrObjectArray.Add(MakeShareable(new FClassOrObject{ LOCTEXT("Class", "Class") }));
	ClassOrObjectArray.Add(MakeShareable(new FClassOrObject{ LOCTEXT("Object", "Object") }));

	FString SelectedType;
	ClassOrObjectHandle->GetValueAsDisplayString(SelectedType);
	bool bClassIsSelected = (SelectedType == TEXT("Class"));

	CurrentIndex = bClassIsSelected ? 0 : 1;

	HeaderRow
	.NameContent()
	[
		PropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.HAlign(HAlign_Fill)
	.MaxDesiredWidth(TOptional<float>())
	[
		SAssignNew(CustomSliders, SHorizontalBox)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SVerticalBox)
			
			+ SVerticalBox::Slot()
			.FillHeight(1)
			[
				SNew(SSpacer)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.VAlign(EVerticalAlignment::VAlign_Center)
			[
				SNew(SComboBox<TSharedPtr<FClassOrObject>>)
				.OptionsSource(&ClassOrObjectArray)
				.OnSelectionChanged_Lambda([&](TSharedPtr<FClassOrObject> Value, ESelectInfo::Type)
				{
					CurrentIndex = ClassOrObjectArray.IndexOfByPredicate([&](const TSharedPtr<FClassOrObject>& In) { return In == Value; });
					if (CurrentIndex == INDEX_NONE)
					{
						CurrentIndex = 0;
					}
					UpdateProperty();
				})
				.OnGenerateWidget_Lambda([&](TSharedPtr<FClassOrObject> Value)
				{
					return SNew(STextBlock)
			    		   .Font(IDetailLayoutBuilder::GetDetailFont())
						   .Text(Value->DisplayName);
				})
				.InitiallySelectedItem(ClassOrObjectArray[CurrentIndex])
				[
					SAssignNew(CurrentText, STextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.Text(FText::FromString(SelectedType))
				]
			]
			
			+ SVerticalBox::Slot()
			.FillHeight(1)
			[
				SNew(SSpacer)
			]
		]

		+ SHorizontalBox::Slot()
		[
			SAssignNew(ClassSlider, SHorizontalBox)
			.Visibility(bClassIsSelected ? EVisibility::Visible : EVisibility::Collapsed)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				ClassHandle->CreatePropertyValueWidget()
			]
		]

		+ SHorizontalBox::Slot()
		[
			SAssignNew(ObjectSlider, SHorizontalBox)
			.Visibility(!bClassIsSelected ? EVisibility::Visible : EVisibility::Collapsed)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				ObjectHandle->CreatePropertyValueWidget()
			]
		]
	];
}

void FVoxelWorldGeneratorPickerCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> InPropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
}

void FVoxelWorldGeneratorPickerCustomization::UpdateProperty()
{
	const TSharedPtr<FClassOrObject>& Value = ClassOrObjectArray[CurrentIndex];

	ClassOrObjectHandle->SetValueFromFormattedString(Value->DisplayName.ToString());

	if (Value->DisplayName.ToString() == TEXT("Class"))
	{
		ClassSlider->SetVisibility(EVisibility::Visible);
		ObjectSlider->SetVisibility(EVisibility::Collapsed);
	}
	else
	{
		ClassSlider->SetVisibility(EVisibility::Collapsed);
		ObjectSlider->SetVisibility(EVisibility::Visible);
	}

	CurrentText->SetText(Value->DisplayName);
}

#undef LOCTEXT_NAMESPACE