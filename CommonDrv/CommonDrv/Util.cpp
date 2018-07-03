#include <fltKernel.h>

extern "C"
{
	PVOID NTAPI RtlPcToFileHeader(_In_ PVOID PcValue, _Out_ PVOID *BaseOfImage);

	_Use_decl_annotations_
		NTSTATUS
		UtilForEachProcessor(
			NTSTATUS(*callback_routine)(void *),
			void *context)
	{
		PAGED_CODE();

		const auto number_of_processors =
			KeQueryActiveProcessorCountEx(ALL_PROCESSOR_GROUPS);
		for (ULONG processor_index = 0; processor_index < number_of_processors;
			processor_index++) {
			PROCESSOR_NUMBER processor_number = {};
			auto status =
				KeGetProcessorNumberFromIndex(processor_index, &processor_number);
			if (!NT_SUCCESS(status)) {
				return status;
			}

			// Switch the current processor
			GROUP_AFFINITY affinity = {};
			affinity.Group = processor_number.Group;
			affinity.Mask = 1ull << processor_number.Number;
			GROUP_AFFINITY previous_affinity = {};
			KeSetSystemGroupAffinityThread(&affinity, &previous_affinity);

			// Execute callback
			status = callback_routine(context);

			KeRevertToUserGroupAffinityThread(&previous_affinity);
			if (!NT_SUCCESS(status)) {
				return status;
			}
		}
		return STATUS_SUCCESS;
	}
	//-----------------------------------------------------------------
	PVOID UtilPcToFileHeader(PVOID pc_value)
	{
		void *base = NULL;
		return RtlPcToFileHeader(pc_value, &base);
	}
}