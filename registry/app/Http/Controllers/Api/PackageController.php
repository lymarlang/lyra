<?php

namespace App\Http\Controllers\Api;

use App\Http\Controllers\Controller;
use App\Models\Package;
use App\Models\PackageVersion;
use Illuminate\Http\Request;
use Illuminate\Support\Facades\Storage;
use Illuminate\Support\Facades\Validator;

class PackageController extends Controller
{
    public function index()
    {
        return Package::with('versions')->get();
    }

    public function show($name)
    {
        $package = Package::where('name', $name)->with('versions')->first();
        if (!$package) {
            return response()->json(['error' => 'Package not found'], 404);
        }
        return $package;
    }

    public function showVersion($name, $version)
    {
        $package = Package::where('name', $name)->first();
        if (!$package) {
            return response()->json(['error' => 'Package not found'], 404);
        }

        $packageVersion = PackageVersion::where('package_id', $package->id)
            ->where('version', $version)
            ->first();

        if (!$packageVersion) {
            return response()->json(['error' => 'Version not found'], 404);
        }

        return $packageVersion;
    }

    public function download($name, $version)
    {
        $package = Package::where('name', $name)->first();
        if (!$package) {
            return response()->json(['error' => 'Package not found'], 404);
        }

        $packageVersion = PackageVersion::where('package_id', $package->id)
            ->where('version', $version)
            ->first();

        if (!$packageVersion || !Storage::disk('local')->exists($packageVersion->file_path)) {
            return response()->json(['error' => 'File not found'], 404);
        }

        return Storage::disk('local')->download($packageVersion->file_path, "{$name}-{$version}.tar.gz");
    }

    public function publish(Request $request)
    {
        $validator = Validator::make($request->all(), [
            'name' => 'required|string|regex:/^[a-zA-Z0-9._-]+$/|max:255',
            'version' => 'required|string|regex:/^[a-zA-Z0-9._-]+$/|max:50',
            'file' => 'required|file|max:10240', // Max 10MB
            'description' => 'nullable|string|max:1000',
        ]);

        if ($validator->fails()) {
            return response()->json(['errors' => $validator->errors()], 422);
        }

        $package = Package::firstOrCreate(
            ['name' => $request->name],
            ['description' => $request->description]
        );

        if (PackageVersion::where('package_id', $package->id)->where('version', $request->version)->exists()) {
            return response()->json(['error' => 'Version already exists'], 409);
        }

        $path = $request->file('file')->store('packages');

        $packageVersion = new PackageVersion();
        $packageVersion->package_id = $package->id;
        $packageVersion->version = $request->version;
        $packageVersion->file_path = $path;
        $packageVersion->checksum = hash_file('sha256', $request->file('file')->path());
        $packageVersion->save();

        return response()->json([
            'message' => 'Package published successfully',
            'package' => $package->name,
            'version' => $packageVersion->version
        ], 201);
    }
}
