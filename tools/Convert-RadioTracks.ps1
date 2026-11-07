<#
.SYNOPSIS
  Builds the Radio system's audio from a folder of numbered MP3s ("1. Title.mp3").

.DESCRIPTION
  Safe to run repeatedly:
    1. Decodes each MP3 to a mono WAV in Sounds/Radio/Samples (half the source sample rate), using Windows Media
       Foundation so nothing needs installing. Existing WAVs are kept.
    2. Writes the station's tracks, with durations measured from the audio, to Configs/Radio/AROR_RadioStations.conf.
    3. Reads the GUIDs Workbench generated (.wav.meta, .sig.meta, .acp.meta) and writes them into the audio project
       and the station config. No GUID is invented: anything not registered yet is reported and left out.

  So: run it, register Sounds/Radio in Workbench's Resource Browser so the .meta files exist, then run it again.
#>
param(
	[Parameter(Mandatory = $true)][string]$Source,
	[string]$StationName = 'Antistasi Radio 101.3',
	[string]$InterludePattern = '^(Interlude \d+|Radio Free Altis)$',
	[int]$SongsPerInterlude = 3
)

$ErrorActionPreference = 'Stop'
$addon = Join-Path $PSScriptRoot '..\OptionalAddons\OpenResistanceRadio' | Resolve-Path | Select-Object -ExpandProperty Path
$samplesDir = Join-Path $addon 'Sounds\Radio\Samples'
$acpPath = Join-Path $addon 'Sounds\Radio\AROR_Radio.acp'
$sigPath = Join-Path $addon 'Sounds\Radio\AROR_Radio.sig'
$confPath = Join-Path $addon 'Configs\Radio\AROR_RadioStations.conf'

Add-Type -TypeDefinition @'
using System;
using System.IO;
using System.Runtime.InteropServices;

public static class AROR_Mp3Decoder
{
	[ComImport, Guid("70ae66f2-c809-4e4f-8915-bdcb406b7993"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
	interface IMFSourceReader
	{
		void GetStreamSelection(int stream, out bool selected);
		void SetStreamSelection(int stream, bool selected);
		void GetNativeMediaType(int stream, int index, out IntPtr type);
		void GetCurrentMediaType(int stream, out IntPtr type);
		void SetCurrentMediaType(int stream, IntPtr reserved, IntPtr type);
		void SetCurrentPosition(ref Guid format, IntPtr position);
		void ReadSample(int stream, int control, out int actual, out int flags, out long time, out IntPtr sample);
	}

	[ComImport, Guid("c40a00f2-b93a-4d80-ae8c-5a1c634f58e4"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
	interface IMFSample
	{
		// IMFAttributes (30 methods), unused here but they hold the vtable slots
		void A00(); void A01(); void A02(); void A03(); void A04(); void A05(); void A06(); void A07(); void A08(); void A09();
		void A10(); void A11(); void A12(); void A13(); void A14(); void A15(); void A16(); void A17(); void A18(); void A19();
		void A20(); void A21(); void A22(); void A23(); void A24(); void A25(); void A26(); void A27(); void A28(); void A29();
		void GetSampleFlags(); void SetSampleFlags(); void GetSampleTime(); void SetSampleTime();
		void GetSampleDuration(); void SetSampleDuration(); void GetBufferCount(); void GetBufferByIndex();
		void ConvertToContiguousBuffer(out IMFMediaBuffer buffer);
	}

	[ComImport, Guid("045FA593-8799-42b8-BC8D-8968C6453507"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
	interface IMFMediaBuffer
	{
		void Lock(out IntPtr data, out int maxLength, out int currentLength);
		void Unlock();
	}

	[StructLayout(LayoutKind.Sequential, Pack = 2)]
	struct WaveFormat
	{
		public short Tag; public short Channels; public int Rate; public int BytesPerSecond;
		public short BlockAlign; public short Bits; public short Extra;
	}

	[DllImport("mfplat.dll", ExactSpelling = true, PreserveSig = false)] static extern void MFStartup(int version, int flags);
	[DllImport("mfplat.dll", ExactSpelling = true, PreserveSig = false)] static extern void MFShutdown();
	[DllImport("mfplat.dll", ExactSpelling = true, PreserveSig = false)] static extern void MFCreateMediaType(out IntPtr type);
	[DllImport("mfplat.dll", ExactSpelling = true, PreserveSig = false)] static extern void MFCreateWaveFormatExFromMFMediaType(IntPtr type, out IntPtr format, out int size, int flags);
	[DllImport("mfplat.dll", ExactSpelling = true, PreserveSig = false)] static extern void MFInitMediaTypeFromWaveFormatEx(IntPtr type, ref WaveFormat format, int size);
	[DllImport("mfreadwrite.dll", ExactSpelling = true, PreserveSig = false)] static extern void MFCreateSourceReaderFromURL([MarshalAs(UnmanagedType.LPWStr)] string url, IntPtr attributes, out IMFSourceReader reader);
	[DllImport("ole32.dll")] static extern void CoTaskMemFree(IntPtr memory);

	const int FIRST_AUDIO_STREAM = unchecked((int)0xFFFFFFFD);
	const int END_OF_STREAM = 2;

	//! Decodes to 16-bit mono at half the source rate (averaging each pair of frames) and returns the length in ms
	public static int Decode(string mp3, string wav)
	{
		MFStartup(0x20070, 0);
		try
		{
			IMFSourceReader reader;
			MFCreateSourceReaderFromURL(mp3, IntPtr.Zero, out reader);
			reader.SetStreamSelection(FIRST_AUDIO_STREAM, true);

			IntPtr native;
			reader.GetNativeMediaType(FIRST_AUDIO_STREAM, 0, out native);
			IntPtr formatPointer;
			int formatSize;
			MFCreateWaveFormatExFromMFMediaType(native, out formatPointer, out formatSize, 0);
			WaveFormat source = (WaveFormat)Marshal.PtrToStructure(formatPointer, typeof(WaveFormat));
			CoTaskMemFree(formatPointer);
			Marshal.Release(native);

			WaveFormat pcm = new WaveFormat();
			pcm.Tag = 1;
			pcm.Channels = source.Channels;
			pcm.Rate = source.Rate;
			pcm.Bits = 16;
			pcm.BlockAlign = (short)(pcm.Channels * 2);
			pcm.BytesPerSecond = pcm.Rate * pcm.BlockAlign;

			IntPtr requested;
			MFCreateMediaType(out requested);
			MFInitMediaTypeFromWaveFormatEx(requested, ref pcm, Marshal.SizeOf(typeof(WaveFormat)));
			reader.SetCurrentMediaType(FIRST_AUDIO_STREAM, IntPtr.Zero, requested);
			Marshal.Release(requested);

			int outRate = pcm.Rate / 2;
			long frames = 0;
			using (FileStream stream = File.Create(wav))
			using (BinaryWriter writer = new BinaryWriter(stream))
			{
				writer.Write(new byte[44]);
				int channels = pcm.Channels;
				int sum = 0;
				int summed = 0;
				while (true)
				{
					int actual, flags;
					long time;
					IntPtr samplePointer;
					reader.ReadSample(FIRST_AUDIO_STREAM, 0, out actual, out flags, out time, out samplePointer);
					if (samplePointer != IntPtr.Zero)
					{
						IMFSample sample = (IMFSample)Marshal.GetObjectForIUnknown(samplePointer);
						IMFMediaBuffer buffer;
						sample.ConvertToContiguousBuffer(out buffer);
						IntPtr data;
						int maxLength, length;
						buffer.Lock(out data, out maxLength, out length);
						byte[] bytes = new byte[length];
						Marshal.Copy(data, bytes, 0, length);
						buffer.Unlock();
						Marshal.ReleaseComObject(buffer);
						Marshal.ReleaseComObject(sample);
						Marshal.Release(samplePointer);

						for (int i = 0; i + channels * 2 <= length; i += channels * 2)
						{
							for (int c = 0; c < channels; c++)
							{
								sum += BitConverter.ToInt16(bytes, i + c * 2);
							}

							summed++;
							if (summed == 2)
							{
								writer.Write((short)(sum / (channels * 2)));
								frames++;
								sum = 0;
								summed = 0;
							}
						}
					}

					if ((flags & END_OF_STREAM) != 0)
						break;
				}

				int dataBytes = (int)(frames * 2);
				stream.Position = 0;
				writer.Write(new char[] { 'R', 'I', 'F', 'F' });
				writer.Write(36 + dataBytes);
				writer.Write(new char[] { 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ' });
				writer.Write(16);
				writer.Write((short)1);
				writer.Write((short)1);
				writer.Write(outRate);
				writer.Write(outRate * 2);
				writer.Write((short)2);
				writer.Write((short)16);
				writer.Write(new char[] { 'd', 'a', 't', 'a' });
				writer.Write(dataBytes);
			}

			Marshal.ReleaseComObject(reader);
			return (int)(frames * 1000 / outRate);
		}
		finally
		{
			MFShutdown();
		}
	}
}
'@

function Get-WavLengthMs([string]$path)
{
	$stream = [IO.File]::OpenRead($path)
	try
	{
		$header = New-Object byte[] 44
		[void]$stream.Read($header, 0, 44)
		return [int]([BitConverter]::ToInt32($header, 40) * [long]1000 / [BitConverter]::ToInt32($header, 28))
	}
	finally { $stream.Close() }
}

#! The GUID Workbench generated for a resource, or $null while it is not registered
function Get-Guid([string]$path)
{
	$meta = "$path.meta"
	if (-not (Test-Path -LiteralPath $meta)) { return $null }
	$match = [regex]::Match((Get-Content -LiteralPath $meta -Raw), 'Name "\{([0-9A-Fa-f]{16})\}')
	if ($match.Success) { return $match.Groups[1].Value }
	return $null
}

function Set-TextCrlf([string]$path, [string]$text)
{
	$normal = ($text -replace "`r`n", "`n") -replace "`n", "`r`n"
	[IO.File]::WriteAllText($path, $normal, (New-Object Text.UTF8Encoding($false)))
}

[void](New-Item -ItemType Directory -Force $samplesDir)
[void](New-Item -ItemType Directory -Force (Split-Path $confPath))

$tracks = New-Object System.Collections.ArrayList
$sources = Get-ChildItem -LiteralPath $Source -Filter *.mp3 -File | Sort-Object { [int]($_.Name -replace '\..*$', '') }
$bankIndex = 0
foreach ($mp3 in $sources)
{
	$title = ($mp3.BaseName -replace '^\d+\.\s*', '').Trim()
	$slug = ($title -replace "[^A-Za-z0-9]", '')
	$wavName = 'AROR_Radio_{0:00}_{1}.wav' -f ($bankIndex + 1), $slug
	$wav = Join-Path $samplesDir $wavName
	if (Test-Path -LiteralPath $wav)
	{
		$length = Get-WavLengthMs $wav
		Write-Host ("kept     {0} ({1} ms)" -f $wavName, $length)
	}
	else
	{
		$length = [AROR_Mp3Decoder]::Decode($mp3.FullName, $wav)
		Write-Host ("decoded  {0} ({1} ms)" -f $wavName, $length)
	}

	[void]$tracks.Add([pscustomobject]@{
		Title = $title; Wav = $wavName; WavPath = $wav; Ms = $length; Bank = $bankIndex
		Interlude = ($title -match $InterludePattern); Guid = (Get-Guid $wav)
	})
	$bankIndex++
}

# Audio project: this station's samples in the Music bank, and our own signal once it is registered
$acpGuid = Get-Guid $acpPath
$sigGuid = Get-Guid $sigPath
$acp = (Get-Content -LiteralPath $acpPath -Raw) -replace "`r`n", "`n"
$samples = New-Object Text.StringBuilder
foreach ($track in $tracks)
{
	if (-not $track.Guid) { continue }
	[void]$samples.Append("    AudioBankSampleClass `"$($track.Wav)`" {`n")
	[void]$samples.Append("     Filename `"{$($track.Guid)}Sounds/Radio/Samples/$($track.Wav)`"`n")
	[void]$samples.Append("     Probability 100`n     Index $($track.Bank)`n     SampleMetadata `"`"`n    }`n")
}
$acp = [regex]::Replace($acp, '(name "Music"[\s\S]*?Samples \{\n)[\s\S]*?(   \}\n  \}\n  BankLocalClass)', {
	param($m) $m.Groups[1].Value + $samples.ToString() + $m.Groups[2].Value
})
if ($sigGuid)
{
	$acp = [regex]::Replace($acp, '\{[0-9A-Fa-f]{16}\}Sounds/(RadioBroadcast/RadioBroadcast|Radio/AROR_Radio)\.sig', "{$sigGuid}Sounds/Radio/AROR_Radio.sig")
}
Set-TextCrlf $acpPath $acp

# Station config. Component and object IDs are derived from the track, so regenerating keeps the diff small
$project = ''
if ($acpGuid) { $project = "{$acpGuid}Sounds/Radio/AROR_Radio.acp" }
$conf = New-Object Text.StringBuilder
[void]$conf.Append("ORRadio_Stations {`n m_aStations {`n  ORRadio_Station `"{6D1A7E2C40000100}`" {`n")
[void]$conf.Append("   m_sName `"$StationName`"`n   m_sAudioProject `"$project`"`n   m_iSongsPerInterlude $SongsPerInterlude`n   m_aTracks {`n")
foreach ($track in $tracks)
{
	$id = '6D1A7E2C4001{0:X4}' -f $track.Bank
	$interlude = 0
	if ($track.Interlude) { $interlude = 1 }
	[void]$conf.Append("    ORRadio_Track `"{$id}`" {`n     m_sTitle `"$($track.Title)`"`n     m_iDurationMs $($track.Ms)`n     m_iBankIndex $($track.Bank)`n     m_bInterlude $interlude`n    }`n")
}
[void]$conf.Append("   }`n  }`n }`n}`n")
Set-TextCrlf $confPath $conf.ToString()

$missing = @($tracks | Where-Object { -not $_.Guid })
$totalMs = ($tracks | Measure-Object -Property Ms -Sum).Sum
$bytes = (Get-ChildItem -LiteralPath $samplesDir -Filter *.wav | Measure-Object -Property Length -Sum).Sum
Write-Host ("{0} tracks, {1:N1} minutes, {2:N0} MB of WAV" -f $tracks.Count, ($totalMs / 60000), ($bytes / 1MB))
if ($missing.Count -gt 0 -or -not $acpGuid -or -not $sigGuid)
{
	Write-Host "Not linked yet: $($missing.Count) WAVs, audio project registered: $([bool]$acpGuid), signal registered: $([bool]$sigGuid)."
	Write-Host "Register Sounds/Radio in Workbench's Resource Browser, then run this again."
}
else
{
	Write-Host 'All audio is linked. Reload the config in Workbench.'
}
