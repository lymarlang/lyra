<?php

namespace App\Models;

use Illuminate\Database\Eloquent\Model;

class PackageVersion extends Model
{
    protected $fillable = ['package_id', 'version', 'checksum', 'file_path'];

    public function package()
    {
        return $this->belongsTo(Package::class);
    }
}
