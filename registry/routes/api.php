<?php

use App\Http\Controllers\Api\PackageController;
use Illuminate\Http\Request;
use Illuminate\Support\Facades\Route;

Route::get('/packages', [PackageController::class, 'index']);
Route::get('/packages/{name}', [PackageController::class, 'show']);
Route::get('/packages/{name}/{version}', [PackageController::class, 'showVersion']);
Route::get('/packages/{name}/{version}/download', [PackageController::class, 'download']);
Route::post('/publish', [PackageController::class, 'publish']);

Route::get('/user', function (Request $request) {
    return $request->user();
})->middleware('auth:sanctum');
